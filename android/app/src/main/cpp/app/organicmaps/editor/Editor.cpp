#include <jni.h>

#include "app/organicmaps/core/jni_helper.hpp"
#include "app/organicmaps/Framework.hpp"

#include "editor/osm_editor.hpp"

#include "indexer/cuisines.hpp"
#include "indexer/editable_map_object.hpp"
#include "indexer/feature_utils.hpp"
#include "indexer/validate_and_format_contacts.hpp"

#include "coding/string_utf8_multilang.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"
#include "base/string_utils.hpp"

#include "std/target_os.hpp"

#include <algorithm>
#include <set>
#include <vector>

namespace
{
using TCuisine = std::pair<std::string, std::string>;
osm::EditableMapObject g_editableMapObject;

jclass g_localNameClazz;
jmethodID g_localNameCtor;
jfieldID g_localNameFieldCode;
jfieldID g_localNameFieldName;
jclass g_localStreetClazz;
jmethodID g_localStreetCtor;
jfieldID g_localStreetFieldDef;
jfieldID g_localStreetFieldLoc;
jclass g_namesDataSourceClassID;
jmethodID g_namesDataSourceConstructorID;

jobject ToJavaName(JNIEnv * env, osm::LocalizedName const & name)
{
  jni::TScopedLocalRef jName(env, jni::ToJavaString(env, name.m_name));
  jni::TScopedLocalRef jLang(env, jni::ToJavaString(env, name.m_lang));
  jni::TScopedLocalRef jLangName(env, jni::ToJavaString(env, name.m_langName));
  return env->NewObject(g_localNameClazz, g_localNameCtor, name.m_code,
                        jName.get(), jLang.get(), jLangName.get());
}

jobject ToJavaStreet(JNIEnv * env, osm::LocalizedStreet const & street)
{
  return env->NewObject(g_localStreetClazz, g_localStreetCtor,
                        jni::TScopedLocalRef(env, jni::ToJavaString(env, street.m_defaultName)).get(),
                        jni::TScopedLocalRef(env, jni::ToJavaString(env, street.m_localizedName)).get());
}

osm::NewFeatureCategories & GetFeatureCategories()
{
  static osm::NewFeatureCategories categories = g_framework->NativeFramework()->GetEditorCategories();
  return categories;
}
}  // namespace

extern "C"
{
using osm::Editor;

JNIEXPORT void JNICALL
Java_app_organicmaps_editor_Editor_nativeInit(JNIEnv * env, jclass)
{
  g_localNameClazz = jni::GetGlobalClassRef(env, "app/organicmaps/editor/data/LocalizedName");
  // LocalizedName(int code, @NonNull String name, @NonNull String lang, @NonNull String langName)
  g_localNameCtor = jni::GetConstructorID(env, g_localNameClazz, "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
  g_localNameFieldCode = env->GetFieldID(g_localNameClazz, "code", "I");
  g_localNameFieldName = env->GetFieldID(g_localNameClazz, "name", "Ljava/lang/String;");

  g_localStreetClazz = jni::GetGlobalClassRef(env, "app/organicmaps/editor/data/LocalizedStreet");
  // LocalizedStreet(@NonNull String defaultName, @NonNull String localizedName)
  g_localStreetCtor = jni::GetConstructorID(env, g_localStreetClazz, "(Ljava/lang/String;Ljava/lang/String;)V");
  g_localStreetFieldDef = env->GetFieldID(g_localStreetClazz, "defaultName", "Ljava/lang/String;");
  g_localStreetFieldLoc = env->GetFieldID(g_localStreetClazz, "localizedName", "Ljava/lang/String;");

  g_namesDataSourceClassID = jni::GetGlobalClassRef(env, "app/organicmaps/editor/data/NamesDataSource");
  g_namesDataSourceConstructorID = jni::GetConstructorID(env, g_namesDataSourceClassID, "([Lapp/organicmaps/editor/data/LocalizedName;I)V");
}

JNIEXPORT jstring JNICALL
Java_app_organicmaps_editor_Editor_nativeGetOpeningHours(JNIEnv * env, jclass)
{
  return jni::ToJavaString(env, g_editableMapObject.GetOpeningHours());
}

JNIEXPORT void JNICALL
Java_app_organicmaps_editor_Editor_nativeSetOpeningHours(JNIEnv * env, jclass, jstring value)
{
  g_editableMapObject.SetOpeningHours(jni::ToNativeString(env, value));
}

JNIEXPORT jstring JNICALL
Java_app_organicmaps_editor_Editor_nativeGetMetadata(JNIEnv * env, jclass, jint id)
{
  auto const metaID = static_cast<osm::MapObject::MetadataID>(id);
  ASSERT_LESS(metaID, osm::MapObject::MetadataID::FMD_COUNT, ());
  if (osm::isSocialContactTag(metaID))
  {
    auto const value = g_editableMapObject.GetMetadata(metaID);
    if (value.find('/') == std::string::npos) // `value` contains pagename.
      return jni::ToJavaString(env, value);
    // `value` contains URL.
    return jni::ToJavaString(env, osm::socialContactToURL(metaID, value));
  }
  return jni::ToJavaString(env, g_editableMapObject.GetMetadata(metaID));
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_editor_Editor_nativeIsMetadataValid(JNIEnv * env, jclass, jint id, jstring value)
{
  auto const metaID = static_cast<osm::MapObject::MetadataID>(id);
  ASSERT_LESS(metaID, osm::MapObject::MetadataID::FMD_COUNT, ());
  return osm::EditableMapObject::IsValidMetadata(metaID, jni::ToNativeString(env, value));
}

JNIEXPORT void JNICALL
Java_app_organicmaps_editor_Editor_nativeSetMetadata(JNIEnv * env, jclass, jint id, jstring value)
{
  auto const metaID = static_cast<osm::MapObject::MetadataID>(id);
  ASSERT_LESS(metaID, osm::MapObject::MetadataID::FMD_COUNT, ());
  g_editableMapObject.SetMetadata(metaID, jni::ToNativeString(env, value));
}

JNIEXPORT jint JNICALL
Java_app_organicmaps_editor_Editor_nativeGetStars(JNIEnv * env, jclass)
{
  return g_editableMapObject.GetStars();
}

JNIEXPORT jint JNICALL
Java_app_organicmaps_editor_Editor_nativeGetMaxEditableBuildingLevels(JNIEnv *, jclass)
{
  return osm::EditableMapObject::kMaximumLevelsEditableByUsers;
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_editor_Editor_nativeHasWifi(JNIEnv *, jclass)
{
  return g_editableMapObject.GetInternet() == feature::Internet::Wlan;
}

JNIEXPORT void JNICALL
Java_app_organicmaps_editor_Editor_nativeSetHasWifi(JNIEnv *, jclass, jboolean hasWifi)
{
  if (hasWifi != (g_editableMapObject.GetInternet() == feature::Internet::Wlan))
    g_editableMapObject.SetInternet(hasWifi ? feature::Internet::Wlan : feature::Internet::Unknown);
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_editor_Editor_nativeSaveEditedFeature(JNIEnv *, jclass)
{
  switch (g_framework->NativeFramework()->SaveEditedMapObject(g_editableMapObject))
  {
  case osm::Editor::SaveResult::NothingWasChanged:
  case osm::Editor::SaveResult::SavedSuccessfully:
    return true;
  case osm::Editor::SaveResult::NoFreeSpaceError:
  case osm::Editor::SaveResult::NoUnderlyingMapError:
  case osm::Editor::SaveResult::SavingError:
    return false;
  }
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_editor_Editor_nativeShouldShowEditPlace(JNIEnv *, jclass)
{
  ::Framework * frm = g_framework->NativeFramework();
  if (!frm->HasPlacePageInfo())
    return static_cast<jboolean>(false);

  return g_framework->GetPlacePageInfo().ShouldShowEditPlace();
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_editor_Editor_nativeShouldShowAddBusiness(JNIEnv *, jclass)
{
  ::Framework * frm = g_framework->NativeFramework();
  if (!frm->HasPlacePageInfo())
    return static_cast<jboolean>(false);

  return g_framework->GetPlacePageInfo().ShouldShowAddBusiness();
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_editor_Editor_nativeShouldShowAddPlace(JNIEnv *, jclass)
{
  ::Framework * frm = g_framework->NativeFramework();
  if (!frm->HasPlacePageInfo())
    return static_cast<jboolean>(false);

  return g_framework->GetPlacePageInfo().ShouldShowAddPlace();
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_editor_Editor_nativeShouldEnableEditPlace(JNIEnv *, jclass)
{
  ::Framework * frm = g_framework->NativeFramework();
  if (!frm->HasPlacePageInfo())
    return static_cast<jboolean>(false);

  return g_framework->GetPlacePageInfo().ShouldEnableEditPlace();
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_editor_Editor_nativeShouldEnableAddPlace(JNIEnv *, jclass)
{
  ::Framework * frm = g_framework->NativeFramework();
  if (!frm->HasPlacePageInfo())
    return static_cast<jboolean>(false);

  return g_framework->GetPlacePageInfo().ShouldEnableAddPlace();
}

JNIEXPORT jintArray JNICALL
Java_app_organicmaps_editor_Editor_nativeGetEditableProperties(JNIEnv * env, jclass clazz)
{
  auto const & editable = g_editableMapObject.GetEditableProperties();
  size_t const size = editable.size();
  jintArray jEditableFields = env->NewIntArray(static_cast<jsize>(size));
  jint * arr = env->GetIntArrayElements(jEditableFields, 0);
  for (size_t i = 0; i < size; ++i)
    arr[i] = base::Underlying(editable[i]);
  env->ReleaseIntArrayElements(jEditableFields, arr, 0);

  return jEditableFields;
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_editor_Editor_nativeIsAddressEditable(JNIEnv * env, jclass clazz)
{
  return g_editableMapObject.IsAddressEditable();
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_editor_Editor_nativeIsNameEditable(JNIEnv * env, jclass clazz)
{
  return g_editableMapObject.IsNameEditable();
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_editor_Editor_nativeIsPointType(JNIEnv * env, jclass clazz)
{
  return g_editableMapObject.IsPointType();
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_editor_Editor_nativeIsBuilding(JNIEnv * env, jclass clazz)
{
  return g_editableMapObject.IsBuilding();
}

JNIEXPORT jobject JNICALL
Java_app_organicmaps_editor_Editor_nativeGetNamesDataSource(JNIEnv * env, jclass)
{
  auto const namesDataSource = g_editableMapObject.GetNamesDataSource();

  jobjectArray names = jni::ToJavaArray(env, g_localNameClazz, namesDataSource.names, ToJavaName);
  jsize const mandatoryNamesCount = static_cast<jsize>(namesDataSource.mandatoryNamesCount);

  return env->NewObject(g_namesDataSourceClassID, g_namesDataSourceConstructorID, names, mandatoryNamesCount);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_editor_Editor_nativeSetNames(JNIEnv * env, jclass, jobjectArray names)
{
  int const length = env->GetArrayLength(names);
  for (int i = 0; i < length; i++)
  {
    auto jName = env->GetObjectArrayElement(names, i);
    g_editableMapObject.SetName(jni::ToNativeString(env, static_cast<jstring>(env->GetObjectField(jName, g_localNameFieldName))),
                                env->GetIntField(jName, g_localNameFieldCode));
  }
}

JNIEXPORT jobject JNICALL
Java_app_organicmaps_editor_Editor_nativeGetStreet(JNIEnv * env, jclass)
{
  return ToJavaStreet(env, g_editableMapObject.GetStreet());
}

JNIEXPORT void JNICALL
Java_app_organicmaps_editor_Editor_nativeSetStreet(JNIEnv * env, jclass, jobject street)
{
  g_editableMapObject.SetStreet({jni::ToNativeString(env, (jstring) env->GetObjectField(street, g_localStreetFieldDef)),
                                 jni::ToNativeString(env, (jstring) env->GetObjectField(street, g_localStreetFieldLoc))});
}
JNIEXPORT jobjectArray JNICALL
Java_app_organicmaps_editor_Editor_nativeGetNearbyStreets(JNIEnv * env, jclass clazz)
{
  return jni::ToJavaArray(env, g_localStreetClazz, g_editableMapObject.GetNearbyStreets(), ToJavaStreet);
}

JNIEXPORT jobjectArray JNICALL
Java_app_organicmaps_editor_Editor_nativeGetSupportedLanguages(JNIEnv * env, jclass clazz, jboolean includeServiceLangs)
{
  using TLang = StringUtf8Multilang::Lang;
  //public Language(@NonNull String code, @NonNull String name)
  static jclass const langClass = jni::GetGlobalClassRef(env, "app/organicmaps/editor/data/Language");
  static jmethodID const langCtor = jni::GetConstructorID(env, langClass, "(Ljava/lang/String;Ljava/lang/String;)V");

  return jni::ToJavaArray(env, langClass, StringUtf8Multilang::GetSupportedLanguages(includeServiceLangs),
                          [](JNIEnv * env, TLang const & lang)
                          {
                            jni::TScopedLocalRef const code(env, jni::ToJavaString(env, lang.m_code));
                            jni::TScopedLocalRef const name(env, jni::ToJavaString(env, lang.m_name));
                            return env->NewObject(langClass, langCtor, code.get(), name.get());
                          });
}

JNIEXPORT jstring JNICALL
Java_app_organicmaps_editor_Editor_nativeGetHouseNumber(JNIEnv * env, jclass)
{
  return jni::ToJavaString(env, g_editableMapObject.GetHouseNumber());
}

JNIEXPORT void JNICALL
Java_app_organicmaps_editor_Editor_nativeSetHouseNumber(JNIEnv * env, jclass, jstring houseNumber)
{
  g_editableMapObject.SetHouseNumber(jni::ToNativeString(env, houseNumber));
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_editor_Editor_nativeHasSomethingToUpload(JNIEnv * env, jclass clazz)
{
  return Editor::Instance().HaveMapEditsOrNotesToUpload();
}

JNIEXPORT void JNICALL
Java_app_organicmaps_editor_Editor_nativeUploadChanges(JNIEnv * env, jclass clazz, jstring token, jstring appVersion, jstring appId)
{
  // TODO: Handle upload status in callback
  Editor::Instance().UploadChanges(jni::ToNativeString(env, token),
      {{"created_by", "Organic Maps " OMIM_OS_NAME " " + jni::ToNativeString(env, appVersion)},
       {"bundle_id", jni::ToNativeString(env, appId)}}, nullptr);
}

JNIEXPORT jlongArray JNICALL
Java_app_organicmaps_editor_Editor_nativeGetStats(JNIEnv * env, jclass clazz)
{
  auto const stats = Editor::Instance().GetStats();
  jlongArray result = env->NewLongArray(3);
  jlong buf[] = {static_cast<jlong>(stats.m_edits.size()), static_cast<jlong>(stats.m_uploadedCount),
                 stats.m_lastUploadTimestamp};
  env->SetLongArrayRegion(result, 0, 3, buf);
  return result;
}

JNIEXPORT void JNICALL
Java_app_organicmaps_editor_Editor_nativeClearLocalEdits(JNIEnv * env, jclass clazz)
{
  Editor::Instance().ClearAllLocalEdits();
}

JNIEXPORT void JNICALL
Java_app_organicmaps_editor_Editor_nativeStartEdit(JNIEnv *, jclass)
{
  ::Framework * frm = g_framework->NativeFramework();
  if (!frm->HasPlacePageInfo())
  {
    ASSERT(g_editableMapObject.GetEditingLifecycle() == osm::EditingLifecycle::CREATED,
           ("PlacePageInfo should only be empty for new features."));
    return;
  }

  place_page::Info const & info = g_framework->GetPlacePageInfo();
  CHECK(frm->GetEditableMapObject(info.GetID(), g_editableMapObject), ("Invalid feature in the place page."));
}

JNIEXPORT void JNICALL
Java_app_organicmaps_editor_Editor_nativeCreateMapObject(JNIEnv * env, jclass,
                                                             jstring featureType)
{
  ::Framework * frm = g_framework->NativeFramework();
  auto const type = classif().GetTypeByReadableObjectName(jni::ToNativeString(env, featureType));
  CHECK(frm->CreateMapObject(frm->GetViewportCenter(), type, g_editableMapObject),
        ("Couldn't create mapobject, wrong coordinates of missing mwm"));
}

// static void nativeCreateNote(String text);
JNIEXPORT void JNICALL
Java_app_organicmaps_editor_Editor_nativeCreateNote(JNIEnv * env, jclass clazz, jstring text)
{
  g_framework->NativeFramework()->CreateNote(
      g_editableMapObject, osm::Editor::NoteProblemType::General, jni::ToNativeString(env, text));
}

// static void nativePlaceDoesNotExist(String comment);
JNIEXPORT void JNICALL
Java_app_organicmaps_editor_Editor_nativePlaceDoesNotExist(JNIEnv * env, jclass clazz, jstring comment)
{
  g_framework->NativeFramework()->CreateNote(g_editableMapObject,
                                             osm::Editor::NoteProblemType::PlaceDoesNotExist,
                                             jni::ToNativeString(env, comment));
}

JNIEXPORT void JNICALL
Java_app_organicmaps_editor_Editor_nativeRollbackMapObject(JNIEnv * env, jclass clazz)
{
  g_framework->NativeFramework()->RollBackChanges(g_editableMapObject.GetID());
}

JNIEXPORT jobjectArray JNICALL
Java_app_organicmaps_editor_Editor_nativeGetAllCreatableFeatureTypes(JNIEnv * env, jclass clazz,
                                                                         jstring jLang)
{
  std::string const & lang = jni::ToNativeString(env, jLang);
  auto & categories = GetFeatureCategories();
  categories.AddLanguage(lang);
  categories.AddLanguage("en");
  return jni::ToJavaStringArray(env, categories.GetAllCreatableTypeNames());
}

JNIEXPORT jobjectArray JNICALL
Java_app_organicmaps_editor_Editor_nativeSearchCreatableFeatureTypes(JNIEnv * env, jclass clazz,
                                                                         jstring query,
                                                                         jstring jLang)
{
  std::string const & lang = jni::ToNativeString(env, jLang);
  auto & categories = GetFeatureCategories();
  categories.AddLanguage(lang);
  categories.AddLanguage("en");
  return jni::ToJavaStringArray(env, categories.Search(jni::ToNativeString(env, query)));
}

JNIEXPORT jobjectArray JNICALL
Java_app_organicmaps_editor_Editor_nativeGetCuisines(JNIEnv * env, jclass clazz)
{
  osm::AllCuisines const & cuisines = osm::Cuisines::Instance().AllSupportedCuisines();
  std::vector<std::string> keys;
  keys.reserve(cuisines.size());
  for (TCuisine const & cuisine : cuisines)
    keys.push_back(cuisine.first);
  return jni::ToJavaStringArray(env, keys);
}

JNIEXPORT jobjectArray JNICALL
Java_app_organicmaps_editor_Editor_nativeGetSelectedCuisines(JNIEnv * env, jclass clazz)
{
  return jni::ToJavaStringArray(env, g_editableMapObject.GetCuisines());
}

JNIEXPORT jobjectArray JNICALL
Java_app_organicmaps_editor_Editor_nativeFilterCuisinesKeys(JNIEnv * env, jclass thiz, jstring jSubstr)
{
  std::string const substr = jni::ToNativeString(env, jSubstr);
  bool const noFilter = substr.length() == 0;
  osm::AllCuisines const & cuisines = osm::Cuisines::Instance().AllSupportedCuisines();
  std::vector<std::string> keys;
  keys.reserve(cuisines.size());

  for (TCuisine const & cuisine : cuisines)
  {
    std::string const & key = cuisine.first;
    std::string const & label = cuisine.second;
    if (noFilter || search::ContainsNormalized(key, substr) || search::ContainsNormalized(label, substr))
      keys.push_back(key);
  }

  return jni::ToJavaStringArray(env, keys);
}

JNIEXPORT jobjectArray JNICALL
Java_app_organicmaps_editor_Editor_nativeTranslateCuisines(JNIEnv * env, jclass clazz, jobjectArray jKeys)
{
  int const length = env->GetArrayLength(jKeys);
  auto const & cuisines = osm::Cuisines::Instance();
  std::vector<std::string> translations;
  translations.reserve(length);
  for (int i = 0; i < length; i++)
  {
    std::string const key = jni::ToNativeString(env, static_cast<jstring>(env->GetObjectArrayElement(jKeys, i)));
    translations.push_back(cuisines.Translate(key));
  }
  return jni::ToJavaStringArray(env, translations);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_editor_Editor_nativeSetSelectedCuisines(JNIEnv * env, jclass clazz, jobjectArray jKeys)
{
  int const length = env->GetArrayLength(jKeys);
  std::vector<std::string> cuisines;
  cuisines.reserve(length);
  for (int i = 0; i < length; i++)
    cuisines.push_back(jni::ToNativeString(env, static_cast<jstring>(env->GetObjectArrayElement(jKeys, i))));
  g_editableMapObject.SetCuisines(cuisines);
}

JNIEXPORT jstring JNICALL
Java_app_organicmaps_editor_Editor_nativeGetFormattedCuisine(JNIEnv * env, jclass clazz)
{
  return jni::ToJavaString(env, g_editableMapObject.FormatCuisines());
}

JNIEXPORT jstring JNICALL
Java_app_organicmaps_editor_Editor_nativeGetMwmName(JNIEnv * env, jclass clazz)
{
  return jni::ToJavaString(env, g_editableMapObject.GetID().GetMwmName());
}

JNIEXPORT jlong JNICALL
Java_app_organicmaps_editor_Editor_nativeGetMwmVersion(JNIEnv * env, jclass clazz)
{
  return g_editableMapObject.GetID().GetMwmVersion();
}

// static boolean nativeIsHouseValid(String houseNumber);
JNIEXPORT jboolean JNICALL
Java_app_organicmaps_editor_Editor_nativeIsHouseValid(JNIEnv * env, jclass clazz, jstring houseNumber)
{
  return osm::EditableMapObject::ValidateHouseNumber(jni::ToNativeString(env, houseNumber));
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_editor_Editor_nativeIsNameValid(JNIEnv * env, jclass clazz, jstring name)
{
  return osm::EditableMapObject::ValidateName(jni::ToNativeString(env, name));
}

JNIEXPORT jstring JNICALL
Java_app_organicmaps_editor_Editor_nativeGetCategory(JNIEnv * env, jclass clazz)
{
  auto types = g_editableMapObject.GetTypes();
  types.SortBySpec();
  return jni::ToJavaString(env, classif().GetReadableObjectName(*types.begin()));
}

// @FeatureStatus
// static native int nativeGetMapObjectStatus();
JNIEXPORT jint JNICALL
Java_app_organicmaps_editor_Editor_nativeGetMapObjectStatus(JNIEnv * env, jclass clazz)
{
  return static_cast<jint>(osm::Editor::Instance().GetFeatureStatus(g_editableMapObject.GetID()));
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_editor_Editor_nativeIsMapObjectUploaded(JNIEnv * env, jclass clazz)
{
  return osm::Editor::Instance().IsFeatureUploaded(g_editableMapObject.GetID().m_mwmId, g_editableMapObject.GetID().m_index);
}

// static nativeMakeLocalizedName(String langCode, String name);
JNIEXPORT jobject JNICALL
Java_app_organicmaps_editor_Editor_nativeMakeLocalizedName(JNIEnv * env, jclass clazz, jstring code, jstring name)
{
  osm::LocalizedName localizedName(jni::ToNativeString(env, code), jni::ToNativeString(env, name));
  return ToJavaName(env, localizedName);
}
} // extern "C"
