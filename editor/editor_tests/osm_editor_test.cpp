#include "testing/testing.hpp"

#include "generator/generator_tests_support/test_feature.hpp"

#include "editor/editor_storage.hpp"
#include "editor/editor_tests/osm_editor_test.hpp"
#include "editor/editor_tests_support/helpers.hpp"
#include "editor/osm_editor.hpp"

#include "search/editor_delegate.hpp"

#include "indexer/classificator.hpp"
#include "indexer/classificator_loader.hpp"
#include "indexer/feature_source.hpp"
#include "indexer/ftypes_matcher.hpp"
#include "indexer/scales.hpp"

#include "platform/platform_tests_support/async_gui_thread.hpp"
#include "platform/platform_tests_support/scoped_file.hpp"

#include "base/logging.hpp"
#include "base/scope_guard.hpp"

#include <memory>

namespace editor
{
namespace testing
{
using namespace generator::tests_support;
using namespace editor::tests_support;
using platform::tests_support::ScopedFile;

namespace
{
class IsCafeChecker : public ftypes::BaseChecker
{
public:
  static IsCafeChecker const & Instance()
  {
    static const IsCafeChecker instance;
    return instance;
  }

private:
  IsCafeChecker()
  {
    Classificator const & c = classif();
    m_types.push_back(c.GetTypeByPath({"amenity", "cafe"}));
  }
};

class TestCafe : public TestPOI
{
public:
  TestCafe(m2::PointD const & center, std::string const & name, std::string const & lang)
    : TestPOI(center, name, lang)
  {
    SetTypes({{"amenity", "cafe"}});
  }
};

class OptionalSaveStorage : public editor::InMemoryStorage
{
public:
  void AllowSave(bool allow)
  {
    m_allowSave = allow;
  }

  // StorageBase overrides:
  bool Save(pugi::xml_document const & doc) override
  {
    if (!m_allowSave)
      return false;

    return InMemoryStorage::Save(doc);
  }

  bool Reset() override
  {
    if (!m_allowSave)
      return false;

    return InMemoryStorage::Reset();
  }

private:
  bool m_allowSave = true;
};

class ScopedOptionalSaveStorage
{
public:
  ScopedOptionalSaveStorage()
  {
    auto & editor = osm::Editor::Instance();
    editor.SetStorageForTesting(std::unique_ptr<OptionalSaveStorage>(m_storage));
  }

  ~ScopedOptionalSaveStorage()
  {
    auto & editor = osm::Editor::Instance();
    editor.SetStorageForTesting(std::make_unique<editor::InMemoryStorage>());
  }

  void AllowSave(bool allow)
  {
    m_storage->AllowSave(allow);
  }

private:
  OptionalSaveStorage * m_storage = new OptionalSaveStorage;
};

template <typename Fn>
void ForEachCafeAtPoint(DataSource & dataSource, m2::PointD const & mercator, Fn && fn)
{
  m2::RectD const rect = mercator::RectByCenterXYAndSizeInMeters(mercator, 0.2 /* rect width */);

  auto const f = [&fn](FeatureType & ft)
  {
    if (IsCafeChecker::Instance()(ft))
    {
      fn(ft);
    }
  };

  dataSource.ForEachInRect(f, rect, scales::GetUpperScale());
}

void FillEditableMapObject(osm::Editor const & editor, FeatureType & ft, osm::EditableMapObject & emo)
{
  emo.SetFromFeatureType(ft);
  emo.SetHouseNumber(ft.GetHouseNumber());
  emo.SetEditableProperties(editor.GetEditableProperties(ft));
}

void SetBuildingLevels(osm::EditableMapObject & emo, std::string s)
{
  emo.SetMetadata(feature::Metadata::FMD_BUILDING_LEVELS, std::move(s));
}

void SetBuildingLevelsToOne(FeatureType & ft)
{
  EditFeature(ft, [](osm::EditableMapObject & emo)
  {
    SetBuildingLevels(emo, "1");
  });
}

void CreateCafeAtPoint(m2::PointD const & point, MwmSet::MwmId const & mwmId, osm::EditableMapObject &emo)
{
  auto & editor = osm::Editor::Instance();

  editor.CreatePoint(classif().GetTypeByPath({"amenity", "cafe"}), point, mwmId, emo);
  emo.SetHouseNumber("12");
  TEST_EQUAL(editor.SaveEditedFeature(emo), osm::Editor::SaveResult::SavedSuccessfully, ());
}

void GenerateUploadedFeature(MwmSet::MwmId const & mwmId,
                             osm::EditableMapObject const & emo,
                             pugi::xml_document & out)
{
  pugi::xml_node root = out.append_child("omaps");
  root.append_attribute("format_version") = 1;

  pugi::xml_node mwmNode = root.append_child("mwm");
  mwmNode.append_attribute("name") = mwmId.GetInfo()->GetCountryName().c_str();
  mwmNode.append_attribute("version") = static_cast<long long>(mwmId.GetInfo()->GetVersion());
  pugi::xml_node created = mwmNode.append_child("create");

  editor::XMLFeature xf = editor::ToXML(emo, true);
  xf.SetMWMFeatureIndex(emo.GetID().m_index);
  xf.SetModificationTime(time(nullptr));
  xf.SetUploadTime(time(nullptr));
  xf.SetUploadStatus("Uploaded");
  xf.AttachToParentNode(created);
}

uint32_t CountFeaturesInRect(MwmSet::MwmId const & mwmId, m2::RectD const & rect)
{
  auto & editor = osm::Editor::Instance();
  int unused = 0;
  uint32_t counter = 0;
  editor.ForEachCreatedFeature(mwmId, [&counter](uint32_t index) { ++counter; }, rect, unused);

  return counter;
}

uint32_t CountFeatureTypeInRectByDataSource(DataSource const & dataSource, m2::RectD const & rect)
{
  auto const scale = scales::GetUpperScale();
  uint32_t counter = 0;
  dataSource.ForEachInRect([&counter](FeatureType const &) { ++counter; }, rect, scale);

  return counter;
}
}  // namespace

EditorTest::EditorTest()
{
  try
  {
    classificator::Load();
  }
  catch (RootException const & e)
  {
    LOG(LERROR, ("Classificator read error: ", e.what()));
  }

  editor::tests_support::SetUpEditorForTesting(std::make_unique<search::EditorDelegate>(m_dataSource));
}

EditorTest::~EditorTest()
{
  editor::tests_support::TearDownEditorForTesting();

  m_dataSource.ClearCache();
  for (auto const & file : m_mwmFiles)
    Cleanup(file);
}

void EditorTest::GetFeatureTypeInfoTest()
{
  auto & editor = osm::Editor::Instance();

  {
    MwmSet::MwmId mwmId;

    TEST(!editor.GetFeatureTypeInfo((*editor.m_features.Get()), mwmId, 0), ());
  }

  auto const mwmId = ConstructTestMwm([](TestMwmBuilder & builder)
  {
    TestCafe cafe(m2::PointD(1.0, 1.0), "London Cafe", "en");
    builder.Add(cafe);
  });

  ForEachCafeAtPoint(m_dataSource, m2::PointD(1.0, 1.0), [&editor](FeatureType & ft)
  {
    auto const featuresBefore = editor.m_features.Get();
    TEST(!editor.GetFeatureTypeInfo(*featuresBefore, ft.GetID().m_mwmId, ft.GetID().m_index), ());

    SetBuildingLevelsToOne(ft);

    auto const featuresAfter = editor.m_features.Get();
    auto const fti = editor.GetFeatureTypeInfo(*featuresAfter, ft.GetID().m_mwmId, ft.GetID().m_index);
    TEST_NOT_EQUAL(fti, nullptr, ());
    TEST_EQUAL(fti->m_object.GetID(), ft.GetID(), ());
  });
}

void EditorTest::GetEditedFeatureTest()
{
  auto & editor = osm::Editor::Instance();

  {
    FeatureID feature;
    TEST(!editor.GetEditedFeature(feature), ());
  }

  auto const mwmId = ConstructTestMwm([](TestMwmBuilder & builder)
  {
    TestCafe cafe(m2::PointD(1.0, 1.0), "London Cafe", "en");
    builder.Add(cafe);
  });

  ForEachCafeAtPoint(m_dataSource, m2::PointD(1.0, 1.0), [&editor](FeatureType & ft) {
    TEST(!editor.GetEditedFeature(ft.GetID()), ());

    SetBuildingLevelsToOne(ft);

    auto savedEmo = editor.GetEditedFeature(ft.GetID());
    TEST(savedEmo, ());

    TEST_EQUAL(savedEmo->GetMetadata(feature::Metadata::FMD_BUILDING_LEVELS), "1", ());

    TEST_EQUAL(ft.GetID(), savedEmo->GetID(), ());
  });
}

void EditorTest::SetIndexTest()
{
  // osm::Editor::SetIndex was called in constructor.
  auto & editor = osm::Editor::Instance();

  auto const gbMwmId = BuildMwm("GB", [](TestMwmBuilder & builder)
  {
    TestCafe cafe({1.0, 1.0}, "London Cafe", "en");
    TestStreet street({{0.0, 0.0}, {1.0, 1.0}, {2.0, 2.0}}, "Test street", "en");
    cafe.SetStreetName(street.GetName("en"));
    builder.Add(street);
    builder.Add(cafe);
    builder.Add(TestCafe({3.0, 3.0}, "London Cafe", "en"));
    builder.Add(TestCafe({4.0, 4.0}, "London Cafe", "en"));
    builder.Add(TestCafe({4.0, 4.0}, "London Cafe", "en"));
    builder.Add(TestCafe({4.0, 4.0}, "London Cafe", "en"));
  });

  auto const mwmId = editor.GetMwmIdByMapName("GB");

  TEST_EQUAL(gbMwmId, mwmId, ());

  osm::EditableMapObject emo;
  CreateCafeAtPoint({2.0, 2.0}, gbMwmId, emo);

  ForEachCafeAtPoint(m_dataSource, m2::PointD(1.0, 1.0), [&editor](FeatureType & ft) {
    auto const firstPtr = editor.GetOriginalMapObject(ft.GetID());
    TEST(firstPtr, ());
    SetBuildingLevelsToOne(ft);
    auto const secondPtr = editor.GetOriginalMapObject(ft.GetID());
    TEST(secondPtr, ());
    TEST_EQUAL(firstPtr->GetID(), secondPtr->GetID(), ());
  });

  ForEachCafeAtPoint(m_dataSource, m2::PointD(1.0, 1.0), [&editor](FeatureType & ft) {
    TEST_EQUAL(editor.GetOriginalFeatureStreet(ft.GetID()), "Test street", ());

    EditFeature(ft, [](osm::EditableMapObject & emo)
    {
      osm::LocalizedStreet ls{"Some street", ""};
      emo.SetStreet(ls);
    });
    TEST_EQUAL(editor.GetOriginalFeatureStreet(ft.GetID()), "Test street", ());
  });

  uint32_t counter = 0;
  editor.ForEachFeatureAtPoint([&counter](FeatureType & ft)
  {
    ++counter;
  }, {100.0, 100.0});

  TEST_EQUAL(counter, 0, ());

  counter = 0;
  editor.ForEachFeatureAtPoint([&counter](FeatureType & ft)
  {
    ++counter;
  }, {3.0, 3.0});

  TEST_EQUAL(counter, 1, ());

  counter = 0;
  editor.ForEachFeatureAtPoint([&counter](FeatureType & ft)
  {
    ++counter;
  }, {1.0, 1.0});

  TEST_EQUAL(counter, 2, ());

  counter = 0;
  editor.ForEachFeatureAtPoint([&counter](FeatureType & ft)
  {
    ++counter;
  }, {4.0, 4.0});

  TEST_EQUAL(counter, 3, ());
}

void EditorTest::GetEditedFeatureStreetTest()
{
  auto & editor = osm::Editor::Instance();

  {
    FeatureID feature;
    std::string street;
    TEST(!editor.GetEditedFeatureStreet(feature, street), ());
  }

  auto const mwmId = ConstructTestMwm([](TestMwmBuilder & builder)
  {
    TestCafe cafe(m2::PointD(1.0, 1.0), "London Cafe", "en");
    builder.Add(cafe);
  });

  ForEachCafeAtPoint(m_dataSource, m2::PointD(1.0, 1.0), [&editor](FeatureType & ft)
  {
    std::string street;
    TEST(!editor.GetEditedFeatureStreet(ft.GetID(), street), ());

    osm::LocalizedStreet ls{"some street", ""};
    EditFeature(ft, [&ls](osm::EditableMapObject & emo)
    {
      emo.SetStreet(ls);
    });

    TEST(editor.GetEditedFeatureStreet(ft.GetID(), street), ());
    TEST_EQUAL(street, ls.m_defaultName, ());
  });
}

void EditorTest::GetFeatureStatusTest()
{
  auto & editor = osm::Editor::Instance();

  auto const mwmId = ConstructTestMwm([](TestMwmBuilder & builder)
  {
    TestCafe cafe(m2::PointD(1.0, 1.0), "London Cafe", "en");
    TestCafe unnamedCafe(m2::PointD(2.0, 2.0), "", "en");

    builder.Add(cafe);
    builder.Add(unnamedCafe);
  });

  ForEachCafeAtPoint(m_dataSource, m2::PointD(1.0, 1.0), [&editor](FeatureType & ft)
  {
    TEST_EQUAL(editor.GetFeatureStatus(ft.GetID()), FeatureStatus::Untouched, ());

    osm::EditableMapObject emo;
    FillEditableMapObject(editor, ft, emo);
    SetBuildingLevels(emo, "1");
    TEST_EQUAL(editor.SaveEditedFeature(emo), osm::Editor::SaveResult::SavedSuccessfully, ());

    TEST_EQUAL(editor.GetFeatureStatus(ft.GetID()), FeatureStatus::Modified, ());
    editor.MarkFeatureAsObsolete(emo.GetID());
    TEST_EQUAL(editor.GetFeatureStatus(emo.GetID()), FeatureStatus::Obsolete, ());
  });

  ForEachCafeAtPoint(m_dataSource, m2::PointD(2.0, 2.0), [&editor](FeatureType & ft)
  {
    TEST_EQUAL(editor.GetFeatureStatus(ft.GetID()), FeatureStatus::Untouched, ());
    editor.DeleteFeature(ft.GetID());
    TEST_EQUAL(editor.GetFeatureStatus(ft.GetID()), FeatureStatus::Deleted, ());
  });

  osm::EditableMapObject emo;
  CreateCafeAtPoint({1.5, 1.5}, mwmId, emo);

  TEST_EQUAL(editor.GetFeatureStatus(emo.GetID()), FeatureStatus::Created, ());
}

void EditorTest::IsFeatureUploadedTest()
{
  auto & editor = osm::Editor::Instance();

  auto const mwmId = ConstructTestMwm([](TestMwmBuilder & builder)
  {
    TestCafe cafe(m2::PointD(1.0, 1.0), "London Cafe", "en");
    builder.Add(cafe);

    builder.Add(TestPOI(m2::PointD(10, 10), "Corner Post", "default"));
  });

  ForEachCafeAtPoint(m_dataSource, m2::PointD(1.0, 1.0), [&editor](FeatureType & ft)
  {
    TEST(!editor.IsFeatureUploaded(ft.GetID().m_mwmId, ft.GetID().m_index), ());
  });

  osm::EditableMapObject emo;
  CreateCafeAtPoint({3.0, 3.0}, mwmId, emo);

  TEST(!editor.IsFeatureUploaded(emo.GetID().m_mwmId, emo.GetID().m_index), ());

  pugi::xml_document doc;
  GenerateUploadedFeature(mwmId, emo, doc);
  editor.m_storage->Save(doc);
  editor.LoadEdits();

  TEST(editor.IsFeatureUploaded(emo.GetID().m_mwmId, emo.GetID().m_index), ());
}

void EditorTest::DeleteFeatureTest()
{
  auto & editor = osm::Editor::Instance();

  auto const mwmId = ConstructTestMwm([](TestMwmBuilder & builder)
  {
    TestCafe cafe(m2::PointD(1.0, 1.0), "London Cafe", "en");
    builder.Add(cafe);

    builder.Add(TestPOI(m2::PointD(10, 10), "Corner Post", "default"));
  });

  osm::EditableMapObject emo;
  CreateCafeAtPoint({3.0, 3.0}, mwmId, emo);

  auto ft = editor.GetEditedFeature(emo.GetID());
  editor.DeleteFeature(ft->GetID());

  TEST_EQUAL(editor.GetFeatureStatus(ft->GetID()), FeatureStatus::Untouched, ());

  ForEachCafeAtPoint(m_dataSource, m2::PointD(1.0, 1.0), [&editor](FeatureType & ft)
  {
    editor.DeleteFeature(ft.GetID());
    TEST_EQUAL(editor.GetFeatureStatus(ft.GetID()), FeatureStatus::Deleted, ());
  });
}

void EditorTest::ClearAllLocalEditsTest()
{
  auto & editor = osm::Editor::Instance();

  auto const mwmId = ConstructTestMwm([](TestMwmBuilder & builder)
  {
    TestCafe cafe(m2::PointD(1.0, 1.0), "London Cafe", "en");
    builder.Add(cafe);

    builder.Add(TestPOI(m2::PointD(10, 10), "Corner Post", "default"));
  });

  osm::EditableMapObject emo;
  CreateCafeAtPoint({3.0, 3.0}, mwmId, emo);

  TEST(!editor.m_features.Get()->empty(), ());
  editor.ClearAllLocalEdits();
  TEST(editor.m_features.Get()->empty(), ());
}

void EditorTest::GetFeaturesByStatusTest()
{
  auto & editor = osm::Editor::Instance();

  {
    MwmSet::MwmId mwmId;
    auto const features = editor.GetFeaturesByStatus(mwmId, FeatureStatus::Untouched);
    TEST(features.empty(), ());
  }

  auto const mwmId = ConstructTestMwm([](TestMwmBuilder & builder)
  {
    TestCafe cafe(m2::PointD(1.0, 1.0), "London Cafe", "en");
    TestCafe unnamedCafe(m2::PointD(2.0, 2.0), "", "en");
    TestCafe someCafe(m2::PointD(3.0, 3.0), "Some cafe", "en");

    builder.Add(cafe);
    builder.Add(unnamedCafe);
    builder.Add(someCafe);

    builder.Add(TestPOI(m2::PointD(10, 10), "Corner Post", "default"));
  });

  FeatureID modifiedId, deletedId, obsoleteId, createdId;

  ForEachCafeAtPoint(m_dataSource, m2::PointD(1.0, 1.0), [&editor, &modifiedId](FeatureType & ft)
  {
    osm::EditableMapObject emo;
    FillEditableMapObject(editor, ft, emo);
    SetBuildingLevels(emo, "1");
    TEST_EQUAL(editor.SaveEditedFeature(emo), osm::Editor::SaveResult::SavedSuccessfully, ());

    modifiedId = emo.GetID();
  });

  ForEachCafeAtPoint(m_dataSource, m2::PointD(2.0, 2.0), [&editor, &deletedId](FeatureType & ft)
  {
    editor.DeleteFeature(ft.GetID());
    deletedId = ft.GetID();
  });

  ForEachCafeAtPoint(m_dataSource, m2::PointD(3.0, 3.0), [&editor, &obsoleteId](FeatureType & ft)
  {
    editor.MarkFeatureAsObsolete(ft.GetID());
    obsoleteId = ft.GetID();
  });

  osm::EditableMapObject emo;
  CreateCafeAtPoint({4.0, 4.0}, mwmId, emo);
  createdId = emo.GetID();

  auto const modified = editor.GetFeaturesByStatus(mwmId, FeatureStatus::Modified);
  auto const deleted = editor.GetFeaturesByStatus(mwmId, FeatureStatus::Deleted);
  auto const obsolete = editor.GetFeaturesByStatus(mwmId, FeatureStatus::Obsolete);
  auto const created = editor.GetFeaturesByStatus(mwmId, FeatureStatus::Created);

  TEST_EQUAL(modified.size(), 1, ());
  TEST_EQUAL(deleted.size(), 1, ());
  TEST_EQUAL(obsolete.size(), 1, ());
  TEST_EQUAL(created.size(), 1, ());

  TEST_EQUAL(modified.front(), modifiedId.m_index, ());
  TEST_EQUAL(deleted.front(), deletedId.m_index, ());
  TEST_EQUAL(obsolete.front(), obsoleteId.m_index, ());
  TEST_EQUAL(created.front(), createdId.m_index, ());
}

void EditorTest::OnMapDeregisteredTest()
{
  auto & editor = osm::Editor::Instance();
  m_dataSource.AddObserver(editor);
  SCOPE_GUARD(removeObs, [&] { m_dataSource.RemoveObserver(editor); });

  auto gbMwmId = BuildMwm("GB", [](TestMwmBuilder & builder)
  {
    TestCafe cafeLondon(m2::PointD(1.0, 1.0), "London Cafe", "en");
    builder.Add(cafeLondon);
  });

  auto const rfMwmId = BuildMwm("RF", [](TestMwmBuilder & builder)
  {
    TestCafe cafeMoscow(m2::PointD(2.0, 2.0), "Moscow Cafe", "en");
    builder.Add(cafeMoscow);
  });

  auto nzMwmId = BuildMwm("NZ", [](TestMwmBuilder & builder)
  {
  });
  m_dataSource.DeregisterMap(nzMwmId.GetInfo()->GetLocalFile().GetCountryFile());

  ForEachCafeAtPoint(m_dataSource, m2::PointD(1.0, 1.0), [](FeatureType & ft)
  {
    SetBuildingLevelsToOne(ft);
  });

  ForEachCafeAtPoint(m_dataSource, m2::PointD(2.0, 2.0), [](FeatureType & ft)
  {
    SetBuildingLevelsToOne(ft);
  });

  TEST_EQUAL(editor.m_features.Get()->size(), 2, (editor.m_features.Get()->size()));

  {
    platform::tests_support::AsyncGuiThread guiThread;
    m_dataSource.DeregisterMap(gbMwmId.GetInfo()->GetLocalFile().GetCountryFile());
  }
  // The map is deregistered but the edits are not deleted until
  // LoadEdits() is called again, either on the next startup or
  // on registering a new map.
  TEST_EQUAL(editor.m_features.Get()->size(), 2, ());

  {
    platform::tests_support::AsyncGuiThread guiThread;
    auto result = m_dataSource.RegisterMap(gbMwmId.GetInfo()->GetLocalFile());
    TEST_EQUAL(result.second, MwmSet::RegResult::Success, ());
    gbMwmId = result.first;
    TEST(gbMwmId.IsAlive(), ());
  }
  // The same map was registered: the edits are still here.
  TEST_EQUAL(editor.m_features.Get()->size(), 2, ());

  {
    platform::tests_support::AsyncGuiThread guiThread;
    m_dataSource.DeregisterMap(gbMwmId.GetInfo()->GetLocalFile().GetCountryFile());
  }
  TEST_EQUAL(editor.m_features.Get()->size(), 2, ());

  {
    platform::tests_support::AsyncGuiThread guiThread;
    auto result = m_dataSource.RegisterMap(nzMwmId.GetInfo()->GetLocalFile());
    TEST_EQUAL(result.second, MwmSet::RegResult::Success, ());
    nzMwmId = result.first;
    TEST(nzMwmId.IsAlive(), ());
  }
  // Another map was registered: all edits are reloaded and
  // the edits for the deleted map are removed.
  TEST_EQUAL(editor.m_features.Get()->size(), 1, ());
  auto const editedMwmId = editor.m_features.Get()->find(rfMwmId);
  bool const result = (editedMwmId != editor.m_features.Get()->end());
  TEST(result, ());
}

void EditorTest::RollBackChangesTest()
{
  auto & editor = osm::Editor::Instance();

  auto const mwmId = ConstructTestMwm([](TestMwmBuilder & builder)
  {
    TestCafe cafe(m2::PointD(1.0, 1.0), "London Cafe", "en");
    builder.Add(cafe);
  });

  std::string const houseNumber = "4a";

  ForEachCafeAtPoint(m_dataSource, m2::PointD(1.0, 1.0), [&editor, &houseNumber](FeatureType & ft)
  {
    osm::EditableMapObject emo;
    FillEditableMapObject(editor, ft, emo);
    emo.SetHouseNumber(houseNumber);
    TEST_EQUAL(editor.SaveEditedFeature(emo), osm::Editor::SaveResult::SavedSuccessfully, ());
  });

  ForEachCafeAtPoint(m_dataSource, m2::PointD(1.0, 1.0), [&houseNumber](FeatureType & ft)
  {
    TEST_EQUAL(ft.GetHouseNumber(), houseNumber, ());
  });

  ForEachCafeAtPoint(m_dataSource, m2::PointD(1.0, 1.0), [&editor](FeatureType & ft)
  {
    editor.RollBackChanges(ft.GetID());
  });

  ForEachCafeAtPoint(m_dataSource, m2::PointD(1.0, 1.0), [](FeatureType & ft)
  {
    TEST_EQUAL(ft.GetHouseNumber(), "", ());
  });
}

void EditorTest::HaveMapEditsOrNotesToUploadTest()
{
  auto & editor = osm::Editor::Instance();

  auto const mwmId = ConstructTestMwm([](TestMwmBuilder & builder)
  {
    TestCafe cafe(m2::PointD(1.0, 1.0), "London Cafe", "en");
    builder.Add(cafe);
  });

  TEST(!editor.HaveMapEditsOrNotesToUpload(), ());

  ForEachCafeAtPoint(m_dataSource, m2::PointD(1.0, 1.0), [](FeatureType & ft)
  {
    SetBuildingLevelsToOne(ft);
  });

  TEST(editor.HaveMapEditsOrNotesToUpload(), ());
  editor.ClearAllLocalEdits();
  TEST(!editor.HaveMapEditsOrNotesToUpload(), ());

  ScopedFile sf("test_notes.xml", ScopedFile::Mode::DoNotCreate);

  editor.m_notes = Notes::MakeNotes(sf.GetFullPath(), true);

  ForEachCafeAtPoint(m_dataSource, m2::PointD(1.0, 1.0), [&editor](FeatureType & ft)
  {
    using NoteType = osm::Editor::NoteProblemType;
    feature::TypesHolder typesHolder;
    std::string defaultName;
    editor.CreateNote({1.0, 1.0}, ft.GetID(), typesHolder, defaultName, NoteType::PlaceDoesNotExist,
                      "exploded");
  });

  TEST(editor.HaveMapEditsOrNotesToUpload(), ());
}

void EditorTest::HaveMapEditsToUploadTest()
{
  auto & editor = osm::Editor::Instance();

  auto const gbMwmId = BuildMwm("GB", [](TestMwmBuilder & builder)
  {
    TestCafe cafeLondon(m2::PointD(1.0, 1.0), "London Cafe", "en");
    builder.Add(cafeLondon);
  });

  auto const rfMwmId = BuildMwm("RF", [](TestMwmBuilder & builder)
  {
    TestCafe cafeMoscow(m2::PointD(2.0, 2.0), "Moscow Cafe", "ru");
    builder.Add(cafeMoscow);
  });

  TEST(!editor.HaveMapEditsToUpload(gbMwmId), ());
  TEST(!editor.HaveMapEditsToUpload(rfMwmId), ());

  ForEachCafeAtPoint(m_dataSource, m2::PointD(1.0, 1.0), [](FeatureType & ft)
  {
    SetBuildingLevelsToOne(ft);
  });

  TEST(editor.HaveMapEditsToUpload(gbMwmId), ());
  TEST(!editor.HaveMapEditsToUpload(rfMwmId), ());

  ForEachCafeAtPoint(m_dataSource, m2::PointD(2.0, 2.0), [](FeatureType & ft)
  {
    SetBuildingLevelsToOne(ft);
  });

  TEST(editor.HaveMapEditsToUpload(gbMwmId), ());
  TEST(editor.HaveMapEditsToUpload(rfMwmId), ());
}

void EditorTest::GetStatsTest()
{
  auto & editor = osm::Editor::Instance();

  auto const mwmId = ConstructTestMwm([](TestMwmBuilder & builder)
  {
    builder.Add(TestCafe(m2::PointD(1.0, 1.0), "London Cafe", "en"));
    builder.Add(TestCafe(m2::PointD(2.0, 2.0), "London Cafe", "en"));
    builder.Add(TestCafe(m2::PointD(3.0, 3.0), "London Cafe", "en"));
    builder.Add(TestCafe(m2::PointD(4.0, 4.0), "London Cafe", "en"));
    builder.Add(TestCafe(m2::PointD(5.0, 5.0), "London Cafe", "en"));

    builder.Add(TestPOI(m2::PointD(10, 10), "Corner Post", "default"));
  });

  auto stats = editor.GetStats();

  TEST(stats.m_edits.empty(), ());
  TEST_EQUAL(stats.m_uploadedCount, 0, ());
  TEST_EQUAL(stats.m_lastUploadTimestamp, base::INVALID_TIME_STAMP, ());

  ForEachCafeAtPoint(m_dataSource, m2::PointD(1.0, 1.0), [](FeatureType & ft)
  {
    SetBuildingLevelsToOne(ft);
  });

  stats = editor.GetStats();
  TEST_EQUAL(stats.m_edits.size(), 1, ());

  ForEachCafeAtPoint(m_dataSource, m2::PointD(4.0, 4.0), [](FeatureType & ft)
  {
    SetBuildingLevelsToOne(ft);
  });

  stats = editor.GetStats();
  TEST_EQUAL(stats.m_edits.size(), 2, ());

  ForEachCafeAtPoint(m_dataSource, m2::PointD(5.0, 5.0), [](FeatureType & ft)
  {
    SetBuildingLevelsToOne(ft);
  });

  stats = editor.GetStats();
  TEST_EQUAL(stats.m_edits.size(), 3, ());
  TEST_EQUAL(stats.m_uploadedCount, 0, ());
  TEST_EQUAL(stats.m_lastUploadTimestamp, base::INVALID_TIME_STAMP, ());

  osm::EditableMapObject emo;
  CreateCafeAtPoint({6.0, 6.0}, mwmId, emo);
  pugi::xml_document doc;
  GenerateUploadedFeature(mwmId, emo, doc);
  editor.m_storage->Save(doc);
  editor.LoadEdits();

  stats = editor.GetStats();
  TEST_EQUAL(stats.m_edits.size(), 1, ());
  TEST_EQUAL(stats.m_uploadedCount, 1, ());
  TEST_NOT_EQUAL(stats.m_lastUploadTimestamp, base::INVALID_TIME_STAMP, ());
}

void EditorTest::IsCreatedFeatureTest()
{
  auto & editor = osm::Editor::Instance();

  auto const mwmId = ConstructTestMwm([](TestMwmBuilder & builder)
  {
    builder.Add(TestCafe(m2::PointD(1.0, 1.0), "London Cafe", "en"));

    builder.Add(TestPOI(m2::PointD(10, 10), "Corner Post", "default"));
  });

  ForEachCafeAtPoint(m_dataSource, m2::PointD(1.0, 1.0), [&editor](FeatureType & ft)
  {
    TEST(!editor.IsCreatedFeature(ft.GetID()), ());
    SetBuildingLevelsToOne(ft);
    TEST(!editor.IsCreatedFeature(ft.GetID()), ());
  });

  osm::EditableMapObject emo;
  TEST(!editor.IsCreatedFeature(emo.GetID()), ());
  CreateCafeAtPoint({2.0, 2.0}, mwmId, emo);
  TEST(editor.IsCreatedFeature(emo.GetID()), ());
}

void EditorTest::ForEachFeatureInMwmRectAndScaleTest()
{
  auto const mwmId = ConstructTestMwm([](TestMwmBuilder & builder)
  {
    builder.Add(TestCafe(m2::PointD(1.0, 1.0), "Untouched Cafe", "en"));
    builder.Add(TestCafe(m2::PointD(3.0, 3.0), "Cafe to modify", "en"));
    builder.Add(TestCafe(m2::PointD(5.0, 5.0), "Cafe to delete", "en"));

    builder.Add(TestPOI(m2::PointD(100.0, 100.0), "Corner Post", "default"));
  });

  ForEachCafeAtPoint(m_dataSource, m2::PointD(3.0, 3.0), [](FeatureType & ft)
  {
    auto & editor = osm::Editor::Instance();
    TEST_EQUAL(editor.GetFeatureStatus(ft.GetID()), FeatureStatus::Untouched, ());

    osm::EditableMapObject emo;
    FillEditableMapObject(editor, ft, emo);
    SetBuildingLevels(emo, "1");
    TEST_EQUAL(editor.SaveEditedFeature(emo), osm::Editor::SaveResult::SavedSuccessfully, ());
    TEST_EQUAL(editor.GetFeatureStatus(ft.GetID()), FeatureStatus::Modified, ());
  });

  ForEachCafeAtPoint(m_dataSource, m2::PointD(5.0, 5.0), [](FeatureType & ft)
  {
    auto & editor = osm::Editor::Instance();
    TEST_EQUAL(editor.GetFeatureStatus(ft.GetID()), FeatureStatus::Untouched, ());
    editor.DeleteFeature(ft.GetID());
    TEST_EQUAL(editor.GetFeatureStatus(ft.GetID()), FeatureStatus::Deleted, ());
  });

  {
    osm::EditableMapObject emo;
    CreateCafeAtPoint({7.0, 7.0}, mwmId, emo);
  }

  {
    osm::EditableMapObject emo;
    CreateCafeAtPoint({9.0, 9.0}, mwmId, emo);
  }

  // Finds created features only.
  TEST_EQUAL(CountFeaturesInRect(mwmId, {0.0, 0.0, 2.0, 2.0}), 0, ());
  TEST_EQUAL(CountFeaturesInRect(mwmId, {2.0, 2.0, 4.0, 4.0}), 0, ());
  TEST_EQUAL(CountFeaturesInRect(mwmId, {4.0, 4.0, 6.0, 6.0}), 0, ());
  TEST_EQUAL(CountFeaturesInRect(mwmId, {6.0, 6.0, 8.0, 8.0}), 1, ());
  TEST_EQUAL(CountFeaturesInRect(mwmId, {8.0, 8.0, 10.0, 10.0}), 1, ());
  TEST_EQUAL(CountFeaturesInRect(mwmId, {0.0, 0.0, 10.0, 10.0}), 2, ());

  // Finds all features except deleted.
  TEST_EQUAL(CountFeatureTypeInRectByDataSource(m_dataSource, {0.0, 0.0, 2.0, 2.0}), 1, ());
  TEST_EQUAL(CountFeatureTypeInRectByDataSource(m_dataSource, {2.0, 2.0, 4.0, 4.0}), 1, ());
  TEST_EQUAL(CountFeatureTypeInRectByDataSource(m_dataSource, {4.0, 4.0, 6.0, 6.0}), 0, ());
  TEST_EQUAL(CountFeatureTypeInRectByDataSource(m_dataSource, {6.0, 6.0, 8.0, 8.0}), 1, ());
  TEST_EQUAL(CountFeatureTypeInRectByDataSource(m_dataSource, {8.0, 8.0, 10.0, 10.0}), 1, ());
  TEST_EQUAL(CountFeatureTypeInRectByDataSource(m_dataSource, {0.0, 0.0, 10.0, 10.0}), 4, ());
}

void EditorTest::CreateNoteTest()
{
  auto & editor = osm::Editor::Instance();

  auto const mwmId = ConstructTestMwm([](TestMwmBuilder & builder)
  {
    builder.Add(TestCafe(m2::PointD(1.0, 1.0), "London Cafe", "en"));
    builder.Add(TestCafe(m2::PointD(2.0, 2.0), "Cafe", "en"));
  });

  auto const createAndCheckNote = [&editor](FeatureID const & fId, ms::LatLon const & pos,
                                            osm::Editor::NoteProblemType const noteType) {
    ScopedFile sf("test_notes.xml", ScopedFile::Mode::DoNotCreate);
    editor.m_notes = Notes::MakeNotes(sf.GetFullPath(), true);
    feature::TypesHolder holder;
    holder.Assign(classif().GetTypeByPath({"amenity", "restaurant"}));
    std::string defaultName = "Test name";
    editor.CreateNote(pos, fId, holder, defaultName, noteType, "with comment");

    auto const notes = editor.m_notes->GetNotes();
    TEST_EQUAL(notes.size(), 1, ());
    auto const & note = notes.front();
    TEST(note.m_point.EqualDxDy(pos, 1e-10), ());
    TEST(note.m_note.find("with comment") != std::string::npos, ());
    TEST(note.m_note.find("OSM snapshot date") != std::string::npos, ());
    TEST(note.m_note.find("restaurant") != std::string::npos, ());
    TEST(note.m_note.find("Test name") != std::string::npos, ());
  };

  // Should match a piece of text in the editor note.
  constexpr char const * kPlaceDoesNotExistMessage = "The place has gone or never existed";

  ForEachCafeAtPoint(m_dataSource, m2::PointD(1.0, 1.0), [&editor, &createAndCheckNote](FeatureType & ft)
  {
    createAndCheckNote(ft.GetID(), {1.0, 1.0}, osm::Editor::NoteProblemType::PlaceDoesNotExist);

    auto notes = editor.m_notes->GetNotes();
    TEST_NOT_EQUAL(notes.front().m_note.find(kPlaceDoesNotExistMessage), std::string::npos, ());
    TEST_EQUAL(editor.GetFeatureStatus(ft.GetID()), FeatureStatus::Obsolete, ());
  });

  ForEachCafeAtPoint(m_dataSource, m2::PointD(2.0, 2.0), [&editor, &createAndCheckNote](FeatureType & ft)
  {
    createAndCheckNote(ft.GetID(), {2.0, 2.0}, osm::Editor::NoteProblemType::General);

    TEST_NOT_EQUAL(editor.GetFeatureStatus(ft.GetID()), FeatureStatus::Obsolete, ());
    auto notes = editor.m_notes->GetNotes();
    TEST_EQUAL(notes.front().m_note.find(kPlaceDoesNotExistMessage), std::string::npos, ());
  });
}

void EditorTest::LoadMapEditsTest()
{
  auto & editor = osm::Editor::Instance();

  auto const gbMwmId = BuildMwm("GB", [](TestMwmBuilder & builder)
  {
    builder.Add(TestCafe(m2::PointD(0.0, 0.0), "London Cafe", "en"));
    builder.Add(TestCafe(m2::PointD(1.0, 1.0), "London Cafe", "en"));

    builder.Add(TestPOI(m2::PointD(100, 100), "Corner Post", "default"));
  });

  auto const rfMwmId = BuildMwm("RF", [](TestMwmBuilder & builder)
  {
    builder.Add(TestCafe(m2::PointD(2.0, 2.0), "Moscow Cafe1", "en"));
    builder.Add(TestCafe(m2::PointD(7.0, 7.0), "Moscow Cafe2", "en"));
    builder.Add(TestCafe(m2::PointD(4.0, 4.0), "Moscow Cafe3", "en"));
    builder.Add(TestCafe(m2::PointD(6.0, 6.0), "Moscow Cafe4", "en"));

    builder.Add(TestPOI(m2::PointD(100, 100), "Corner Post", "default"));
  });

  std::vector<FeatureID> features;

  ForEachCafeAtPoint(m_dataSource, m2::PointD(0.0, 0.0), [&features](FeatureType & ft)
  {
    SetBuildingLevelsToOne(ft);
    features.emplace_back(ft.GetID());
  });

  ForEachCafeAtPoint(m_dataSource, m2::PointD(1.0, 1.0), [&editor, &features](FeatureType & ft)
  {
    editor.DeleteFeature(ft.GetID());
    features.emplace_back(ft.GetID());
  });

  ForEachCafeAtPoint(m_dataSource, m2::PointD(2.0, 2.0), [&editor, &features](FeatureType & ft)
  {
    editor.MarkFeatureAsObsolete(ft.GetID());
    features.emplace_back(ft.GetID());
  });

  ForEachCafeAtPoint(m_dataSource, m2::PointD(7.0, 7.0), [&features](FeatureType & ft)
  {
    SetBuildingLevelsToOne(ft);
    features.emplace_back(ft.GetID());
  });

  ForEachCafeAtPoint(m_dataSource, m2::PointD(4.0, 4.0), [&editor, &features](FeatureType & ft)
  {
    editor.DeleteFeature(ft.GetID());
    features.emplace_back(ft.GetID());
  });

  ForEachCafeAtPoint(m_dataSource, m2::PointD(6.0, 6.0), [&features](FeatureType & ft)
  {
    SetBuildingLevelsToOne(ft);
    features.emplace_back(ft.GetID());
  });

  osm::EditableMapObject emo;
  CreateCafeAtPoint({5.0, 5.0}, rfMwmId, emo);
  features.emplace_back(emo.GetID());

  editor.Save((*editor.m_features.Get()));
  editor.LoadEdits();

  auto const fillLoaded = [&editor](std::vector<FeatureID> & loadedFeatures)
  {
    loadedFeatures.clear();
    for (auto const & mwm : *(editor.m_features.Get()))
    {
      for (auto const & index : mwm.second)
      {
        loadedFeatures.emplace_back(index.second.m_object.GetID());
      }
    }
  };

  std::vector<FeatureID> loadedFeatures;
  fillLoaded(loadedFeatures);

  sort(loadedFeatures.begin(), loadedFeatures.end());
  sort(features.begin(), features.end());
  TEST_EQUAL(features, loadedFeatures, ());

  m_dataSource.DeregisterMap(m_mwmFiles.back().GetCountryFile());
  TEST(RemoveMwm(rfMwmId), ());

  auto const newRfMwmId = BuildMwm("RF", [](TestMwmBuilder & builder)
  {
    builder.Add(TestCafe(m2::PointD(2.0, 2.0), "Moscow Cafe1", "en"));
    builder.Add(TestCafe(m2::PointD(7.0, 7.0), "Moscow Cafe2", "en"));
    builder.Add(TestCafe(m2::PointD(4.0, 4.0), "Moscow Cafe3", "en"));
    builder.Add(TestCafe(m2::PointD(6.0, 6.0), "Moscow Cafe4", "en"));
  }, 1);

  editor.LoadEdits();
  fillLoaded(loadedFeatures);

  TEST_EQUAL(features.size(), loadedFeatures.size(), ());

  m_dataSource.DeregisterMap(m_mwmFiles.back().GetCountryFile());
  TEST(RemoveMwm(newRfMwmId), ());

  TEST_EQUAL(editor.m_features.Get()->size(), 2, ());

  editor.LoadEdits();
  fillLoaded(loadedFeatures);

  TEST_EQUAL(editor.m_features.Get()->size(), 1, ());
  TEST_EQUAL(loadedFeatures.size(), 2, ());

  osm::EditableMapObject gbEmo;
  CreateCafeAtPoint({3.0, 3.0}, gbMwmId, gbEmo);

  pugi::xml_document doc;
  GenerateUploadedFeature(gbMwmId, gbEmo, doc);
  editor.m_storage->Save(doc);
  editor.LoadEdits();
  fillLoaded(loadedFeatures);

  TEST_EQUAL(editor.m_features.Get()->size(), 1, ());
  TEST_EQUAL(loadedFeatures.size(), 1, ());

  m_dataSource.DeregisterMap(m_mwmFiles.back().GetCountryFile());
  TEST(RemoveMwm(gbMwmId), ());

  auto const newGbMwmId = BuildMwm("GB", [](TestMwmBuilder & builder)
  {
    builder.Add(TestCafe(m2::PointD(0.0, 0.0), "London Cafe", "en"));
    builder.Add(TestCafe(m2::PointD(1.0, 1.0), "London Cafe", "en"));
  }, 1);

  newGbMwmId.GetInfo()->m_version.SetSecondsSinceEpoch(time(nullptr) + 1);

  editor.LoadEdits();
  TEST(editor.m_features.Get()->empty(), ());
}

void EditorTest::SaveEditedFeatureTest()
{
  auto & editor = osm::Editor::Instance();

  auto const mwmId = ConstructTestMwm([](TestMwmBuilder & builder)
  {
    builder.Add(TestCafe(m2::PointD(1.0, 1.0), "London Cafe1", "en"));

    builder.Add(TestPOI(m2::PointD(10, 10), "Corner Post", "default"));
  });

  osm::EditableMapObject emo;

  editor.CreatePoint(classif().GetTypeByPath({"amenity", "cafe"}), {4.0, 4.0}, mwmId, emo);
  emo.SetHouseNumber("12");
  TEST_EQUAL(editor.GetFeatureStatus(emo.GetID()), FeatureStatus::Untouched, ());
  TEST_EQUAL(editor.SaveEditedFeature(emo), osm::Editor::SaveResult::SavedSuccessfully, ());
  TEST_EQUAL(editor.GetFeatureStatus(emo.GetID()), FeatureStatus::Created, ());
  TEST_EQUAL(editor.SaveEditedFeature(emo), osm::Editor::SaveResult::NothingWasChanged, ());
  TEST_EQUAL(editor.GetFeatureStatus(emo.GetID()), FeatureStatus::Created, ());

  ForEachCafeAtPoint(m_dataSource, m2::PointD(1.0, 1.0), [&editor](FeatureType & ft)
  {
    osm::EditableMapObject emo;
    FillEditableMapObject(editor, ft, emo);
    TEST_EQUAL(editor.SaveEditedFeature(emo), osm::Editor::SaveResult::NothingWasChanged, ());
    TEST_EQUAL(editor.GetFeatureStatus(emo.GetID()), FeatureStatus::Untouched, ());
    emo.SetHouseNumber("4a");
    TEST_EQUAL(editor.SaveEditedFeature(emo), osm::Editor::SaveResult::SavedSuccessfully, ());
    TEST_EQUAL(editor.GetFeatureStatus(emo.GetID()), FeatureStatus::Modified, ());
    TEST_EQUAL(editor.SaveEditedFeature(emo), osm::Editor::SaveResult::NothingWasChanged, ());
    TEST_EQUAL(editor.GetFeatureStatus(emo.GetID()), FeatureStatus::Modified, ());
    emo.SetHouseNumber("");
    TEST_EQUAL(editor.SaveEditedFeature(emo), osm::Editor::SaveResult::SavedSuccessfully, ());
    TEST_EQUAL(editor.GetFeatureStatus(emo.GetID()), FeatureStatus::Untouched, ());
  });
}

void EditorTest::SaveTransactionTest()
{
  ScopedOptionalSaveStorage optionalSaveStorage;
  auto & editor = osm::Editor::Instance();

  auto const mwmId = BuildMwm("GB", [](TestMwmBuilder & builder)
  {
    builder.Add(TestCafe(m2::PointD(1.0, 1.0), "London Cafe1", "en"));
    builder.Add(TestCafe(m2::PointD(4.0, 4.0), "London Cafe2", "en"));

    builder.Add(TestPOI(m2::PointD(6.0, 6.0), "Corner Post", "default"));
  });

  auto const rfMwmId = BuildMwm("RF", [](TestMwmBuilder & builder)
  {
    builder.Add(TestCafe(m2::PointD(10.0, 10.0), "Moscow Cafe1", "en"));
  });

  ForEachCafeAtPoint(m_dataSource, m2::PointD(1.0, 1.0), [](FeatureType & ft)
  {
    SetBuildingLevelsToOne(ft);
  });

  ForEachCafeAtPoint(m_dataSource, m2::PointD(10.0, 10.0), [](FeatureType & ft)
  {
    SetBuildingLevelsToOne(ft);
  });

  auto const features = editor.m_features.Get();

  TEST_EQUAL(features->size(), 2, ());
  TEST_EQUAL(features->begin()->second.size(), 1, ());
  TEST_EQUAL(features->rbegin()->second.size(), 1, ());

  optionalSaveStorage.AllowSave(false);

  {
    auto saveResult = osm::Editor::SaveResult::NothingWasChanged;
    ForEachCafeAtPoint(m_dataSource, m2::PointD(1.0, 1.0), [&saveResult](FeatureType & ft)
    {
      auto & editor = osm::Editor::Instance();

      osm::EditableMapObject emo;
      emo.SetFromFeatureType(ft);
      emo.SetEditableProperties(editor.GetEditableProperties(ft));
      SetBuildingLevels(emo, "5");

      saveResult = editor.SaveEditedFeature(emo);
    });

    TEST_EQUAL(saveResult, osm::Editor::SaveResult::NoFreeSpaceError, ());

    auto const features = editor.m_features.Get();
    auto const mwmIt = features->find(mwmId);

    TEST(mwmIt != features->end(), ());
    TEST_EQUAL(mwmIt->second.size(), 1, ());
  }

  {
    auto saveResult = osm::Editor::SaveResult::NothingWasChanged;
    ForEachCafeAtPoint(m_dataSource, m2::PointD(1.0, 1.0), [&saveResult](FeatureType & ft)
    {
      auto & editor = osm::Editor::Instance();

      osm::EditableMapObject emo;
      emo.SetFromFeatureType(ft);
      emo.SetEditableProperties(editor.GetEditableProperties(ft));
      SetBuildingLevels(emo, "");

      saveResult = editor.SaveEditedFeature(emo);
    });

    TEST_EQUAL(saveResult, osm::Editor::SaveResult::SavingError, ());

    auto const features = editor.m_features.Get();
    auto const mwmIt = features->find(mwmId);

    TEST(mwmIt != features->end(), ());
    TEST_EQUAL(mwmIt->second.size(), 1, ());

    auto const featureInfo = mwmIt->second.begin()->second;
    TEST_EQUAL(featureInfo.m_status, FeatureStatus::Modified, ());
  }

  {
    ForEachCafeAtPoint(m_dataSource, m2::PointD(1.0, 1.0), [&editor](FeatureType & ft)
    {
      editor.DeleteFeature(ft.GetID());
    });

    auto const features = editor.m_features.Get();
    auto const mwmIt = features->find(mwmId);

    TEST(mwmIt != features->end(), ());
    TEST_EQUAL(mwmIt->second.size(), 1, ());

    auto const featureInfo = mwmIt->second.begin()->second;
    TEST_EQUAL(featureInfo.m_status, FeatureStatus::Modified, ());
  }

  {
    ForEachCafeAtPoint(m_dataSource, m2::PointD(1.0, 1.0), [&editor](FeatureType & ft)
    {
      editor.MarkFeatureAsObsolete(ft.GetID());
    });

    auto const features = editor.m_features.Get();
    auto const mwmIt = features->find(mwmId);

    TEST(mwmIt != features->end(), ());
    TEST_EQUAL(mwmIt->second.size(), 1, ());

    auto const featureInfo = mwmIt->second.begin()->second;
    TEST_EQUAL(featureInfo.m_status, FeatureStatus::Modified, ());
  }

  {
    ForEachCafeAtPoint(m_dataSource, m2::PointD(1.0, 1.0), [&editor](FeatureType & ft)
    {
      using NoteType = osm::Editor::NoteProblemType;
      feature::TypesHolder typesHolder;
      std::string defaultName;
      editor.CreateNote({1.0, 1.0}, ft.GetID(), typesHolder, defaultName, NoteType::PlaceDoesNotExist,
                        "exploded");
    });

    TEST_EQUAL(editor.m_notes->NotUploadedNotesCount(), 0, ());

    auto const features = editor.m_features.Get();
    auto const mwmIt = features->find(mwmId);

    TEST(mwmIt != features->end(), ());
    TEST_EQUAL(mwmIt->second.size(), 1, ());

    auto const featureInfo = mwmIt->second.begin()->second;
    TEST_EQUAL(featureInfo.m_status, FeatureStatus::Modified, ());
  }

  {
    ForEachCafeAtPoint(m_dataSource, m2::PointD(1.0, 1.0), [&editor](FeatureType & ft)
    {
      editor.RollBackChanges(ft.GetID());
    });

    auto const features = editor.m_features.Get();
    auto const mwmIt = features->find(mwmId);

    TEST(mwmIt != features->end(), ());
    TEST_EQUAL(mwmIt->second.size(), 1, ());

    auto const featureInfo = mwmIt->second.begin()->second;
    TEST_EQUAL(featureInfo.m_status, FeatureStatus::Modified, ());
  }

  {
    editor.ClearAllLocalEdits();
    TEST_EQUAL(editor.m_features.Get()->size(), 2, ());
  }

  {
    {
      platform::tests_support::AsyncGuiThread guiThread;
      editor.OnMapDeregistered(mwmId.GetInfo()->GetLocalFile());
    }

    auto const features = editor.m_features.Get();
    auto const mwmIt = features->find(mwmId);

    TEST(mwmIt != features->end(), ());
    TEST_EQUAL(mwmIt->second.size(), 1, ());

    auto const featureInfo = mwmIt->second.begin()->second;
    TEST_EQUAL(featureInfo.m_status, FeatureStatus::Modified, ());
  }

  optionalSaveStorage.AllowSave(true);

  editor.ClearAllLocalEdits();
  TEST(editor.m_features.Get()->empty(), ());
}

void EditorTest::Cleanup(platform::LocalCountryFile const & map)
{
  platform::CountryIndexes::DeleteFromDisk(map);
  map.DeleteFromDisk(MapFileType::Map);
}

bool EditorTest::RemoveMwm(MwmSet::MwmId const & mwmId)
{
  auto const & file = mwmId.GetInfo()->GetLocalFile();
  auto const it = find(m_mwmFiles.begin(), m_mwmFiles.end(), file);

  if (it == m_mwmFiles.end())
    return false;

  Cleanup(*it);
  m_mwmFiles.erase(it);
  return true;
}

void EditorTest::LoadExistingEditsXml()
{
  char const * data = R"(
    <?xml version="1.0"?>
    <mapsme format_version="1">
      <mwm name="TestCountry" version="221119">
        <delete />
        <modify />
        <create>
          <node lat="54.0446163" lon="27.6597626" mwm_file_index="4293918720" timestamp="2022-12-09T18:58:28Z">
            <tag k="name:ru" v="xxx" />
            <tag k="amenity" v="bar" />
          </node>
        </create>
        <obsolete />
      </mwm>
    </mapsme>
  )";

  ConstructTestMwm([](editor::testing::TestMwmBuilder &)
  {
  });

  pugi::xml_document doc;
  TEST(doc.load_string(data), ());

  auto memStorage = std::make_unique<editor::InMemoryStorage>();
  memStorage->Save(doc);

  auto & editor = osm::Editor::Instance();
  editor.SetStorageForTesting(std::move(memStorage));

  editor.LoadEdits();
  TEST_EQUAL(editor.m_features.Get()->size(), 1, ());
}

}  // namespace testing
}  // namespace editor

namespace
{
using editor::testing::EditorTest;

UNIT_CLASS_TEST(EditorTest, GetFeatureTypeInfoTest)
{
  EditorTest::GetFeatureTypeInfoTest();
}

UNIT_CLASS_TEST(EditorTest, SetIndexTest)
{
  EditorTest::SetIndexTest();
}

UNIT_CLASS_TEST(EditorTest, GetFeatureStatusTest)
{
  EditorTest::GetFeatureStatusTest();
}

UNIT_CLASS_TEST(EditorTest, IsFeatureUploadedTest)
{
  EditorTest::IsFeatureUploadedTest();
}

UNIT_CLASS_TEST(EditorTest, DeleteFeatureTest)
{
  EditorTest::DeleteFeatureTest();
}

UNIT_CLASS_TEST(EditorTest, ClearAllLocalEditsTest)
{
  EditorTest::ClearAllLocalEditsTest();
}

UNIT_CLASS_TEST(EditorTest, GetEditedFeatureStreetTest)
{
  EditorTest::GetEditedFeatureStreetTest();
}

UNIT_CLASS_TEST(EditorTest, GetEditedFeatureTest)
{
  EditorTest::GetEditedFeatureTest();
}

UNIT_CLASS_TEST(EditorTest, GetFeaturesByStatusTest)
{
  EditorTest::GetFeaturesByStatusTest();
}

UNIT_CLASS_TEST(EditorTest, OnMapDeregisteredTest)
{
  EditorTest::OnMapDeregisteredTest();
}

UNIT_CLASS_TEST(EditorTest, RollBackChangesTest)
{
  EditorTest::RollBackChangesTest();
}

UNIT_CLASS_TEST(EditorTest, HaveMapEditsOrNotesToUploadTest)
{
  EditorTest::HaveMapEditsOrNotesToUploadTest();
}

UNIT_CLASS_TEST(EditorTest, HaveMapEditsToUploadTest)
{
  EditorTest::HaveMapEditsToUploadTest();
}

UNIT_CLASS_TEST(EditorTest, GetStatsTest)
{
  EditorTest::GetStatsTest();
}

UNIT_CLASS_TEST(EditorTest, IsCreatedFeatureTest)
{
  EditorTest::IsCreatedFeatureTest();
}

UNIT_CLASS_TEST(EditorTest, ForEachFeatureInMwmRectAndScaleTest)
{
  EditorTest::ForEachFeatureInMwmRectAndScaleTest();
}

UNIT_CLASS_TEST(EditorTest, CreateNoteTest)
{
  EditorTest::CreateNoteTest();
}

UNIT_CLASS_TEST(EditorTest, LoadMapEditsTest) { EditorTest::LoadMapEditsTest(); }

UNIT_CLASS_TEST(EditorTest, SaveEditedFeatureTest)
{
  EditorTest::SaveEditedFeatureTest();
}

UNIT_CLASS_TEST(EditorTest, SaveTransactionTest) { EditorTest::SaveTransactionTest(); }

UNIT_CLASS_TEST(EditorTest, LoadEditsXml) { LoadExistingEditsXml(); }
}  // namespace
