#include <jni.h>

#include "com/mapswithme/core/jni_helper.hpp"
#include "com/mapswithme/maps/Framework.hpp"

#include "editor/osm_editor.hpp"

#include "indexer/cuisines.hpp"
#include "indexer/editable_map_object.hpp"

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
Java_com_mapswithme_maps_editor_Editor_nativeInit(JNIEnv * env, jclass)
{
  g_localNameClazz = jni::GetGlobalClassRef(env, "com/mapswithme/maps/editor/data/LocalizedName");
  // LocalizedName(int code, @NonNull String name, @NonNull String lang, @NonNull String langName)
  g_localNameCtor = jni::GetConstructorID(env, g_localNameClazz, "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
  g_localNameFieldCode = env->GetFieldID(g_localNameClazz, "code", "I");
  g_localNameFieldName = env->GetFieldID(g_localNameClazz, "name", "Ljava/lang/String;");

  g_localStreetClazz = jni::GetGlobalClassRef(env, "com/mapswithme/maps/editor/data/LocalizedStreet");
  // LocalizedStreet(@NonNull String defaultName, @NonNull String localizedName)
  g_localStreetCtor = jni::GetConstructorID(env, g_localStreetClazz, "(Ljava/lang/String;Ljava/lang/String;)V");
  g_localStreetFieldDef = env->GetFieldID(g_localStreetClazz, "defaultName", "Ljava/lang/String;");
  g_localStreetFieldLoc = env->GetFieldID(g_localStreetClazz, "localizedName", "Ljava/lang/String;");

  g_namesDataSourceClassID = jni::GetGlobalClassRef(env, "com/mapswithme/maps/editor/data/NamesDataSource");
  g_namesDataSourceConstructorID = jni::GetConstructorID(env, g_namesDataSourceClassID, "([Lcom/mapswithme/maps/editor/data/LocalizedName;I)V");
}

JNIEXPORT jstring JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeGetOpeningHours(JNIEnv * env, jclass)
{
  return jni::ToJavaString(env, g_editableMapObject.GetOpeningHours());
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeSetOpeningHours(JNIEnv * env, jclass, jstring value)
{
  g_editableMapObject.SetOpeningHours(jni::ToNativeString(env, value));
}

JNIEXPORT jstring JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeGetPhone(JNIEnv * env, jclass)
{
  return jni::ToJavaString(env, g_editableMapObject.GetPhone());
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeSetPhone(JNIEnv * env, jclass, jstring value)
{
  g_editableMapObject.SetPhone(jni::ToNativeString(env, value));
}

JNIEXPORT jstring JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeGetWebsite(JNIEnv * env, jclass)
{
  return jni::ToJavaString(env, g_editableMapObject.GetWebsite());
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeSetWebsite(JNIEnv * env, jclass, jstring value)
{
  g_editableMapObject.SetWebsite(jni::ToNativeString(env, value));
}

JNIEXPORT jstring JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeGetFacebookPage(JNIEnv * env, jclass)
{
  return jni::ToJavaString(env, g_editableMapObject.GetFacebookPage());
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeSetFacebookPage(JNIEnv * env, jclass, jstring value)
{
  g_editableMapObject.SetFacebookPage(jni::ToNativeString(env, value));
}

JNIEXPORT jstring JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeGetInstagramPage(JNIEnv * env, jclass)
{
  return jni::ToJavaString(env, g_editableMapObject.GetInstagramPage());
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeSetInstagramPage(JNIEnv * env, jclass, jstring value)
{
  g_editableMapObject.SetInstagramPage(jni::ToNativeString(env, value));
}

JNIEXPORT jstring JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeGetTwitterPage(JNIEnv * env, jclass)
{
  return jni::ToJavaString(env, g_editableMapObject.GetTwitterPage());
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeSetTwitterPage(JNIEnv * env, jclass, jstring value)
{
  g_editableMapObject.SetTwitterPage(jni::ToNativeString(env, value));
}

JNIEXPORT jstring JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeGetVkPage(JNIEnv * env, jclass)
{
  return jni::ToJavaString(env, g_editableMapObject.GetVkPage());
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeSetVkPage(JNIEnv * env, jclass, jstring value)
{
  g_editableMapObject.SetVkPage(jni::ToNativeString(env, value));
}

JNIEXPORT jstring JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeGetEmail(JNIEnv * env, jclass)
{
  return jni::ToJavaString(env, g_editableMapObject.GetEmail());
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeSetEmail(JNIEnv * env, jclass, jstring value)
{
  g_editableMapObject.SetEmail(jni::ToNativeString(env, value));
}

JNIEXPORT jint JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeGetStars(JNIEnv * env, jclass)
{
  return g_editableMapObject.GetStars();
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeSetStars(JNIEnv * env, jclass, jint value)
{
  g_editableMapObject.SetStars(value);
}

JNIEXPORT jstring JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeGetOperator(JNIEnv * env, jclass)
{
  return jni::ToJavaString(env, g_editableMapObject.GetOperator());
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeSetOperator(JNIEnv * env, jclass, jstring value)
{
  g_editableMapObject.SetOperator(jni::ToNativeString(env, value));
}

JNIEXPORT jdouble JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeGetElevation(JNIEnv * env, jclass)
{
  double elevation;
  return g_editableMapObject.GetElevation(elevation) ? elevation : -1;
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeSetElevation(JNIEnv * env, jclass, jdouble value)
{
  g_editableMapObject.SetElevation(value);
}

JNIEXPORT jstring JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeGetWikipedia(JNIEnv * env, jclass)
{
  return jni::ToJavaString(env, g_editableMapObject.GetWikipedia());
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeSetWikipedia(JNIEnv * env, jclass, jstring value)
{
  g_editableMapObject.SetWikipedia(jni::ToNativeString(env, value));
}

JNIEXPORT jstring JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeGetFlats(JNIEnv * env, jclass)
{
  return jni::ToJavaString(env, g_editableMapObject.GetFlats());
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeSetFlats(JNIEnv * env, jclass, jstring value)
{
  g_editableMapObject.SetFlats(jni::ToNativeString(env, value));
}

JNIEXPORT jstring JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeGetBuildingLevels(JNIEnv * env, jclass)
{
  return jni::ToJavaString(env, g_editableMapObject.GetBuildingLevels());
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeSetBuildingLevels(JNIEnv * env, jclass, jstring value)
{
  g_editableMapObject.SetBuildingLevels(jni::ToNativeString(env, value));
}

JNIEXPORT jstring JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeGetZipCode(JNIEnv * env, jclass)
{
  return jni::ToJavaString(env, g_editableMapObject.GetPostcode());
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeSetZipCode(JNIEnv * env, jclass clazz, jstring value)
{
  g_editableMapObject.SetPostcode(jni::ToNativeString(env, value));
}

JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeHasWifi(JNIEnv *, jclass)
{
  return g_editableMapObject.GetInternet() == osm::Internet::Wlan;
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeSetHasWifi(JNIEnv *, jclass, jboolean hasWifi)
{
  g_editableMapObject.SetInternet(hasWifi ? osm::Internet::Wlan : osm::Internet::Unknown);
}

JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeSaveEditedFeature(JNIEnv *, jclass)
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
Java_com_mapswithme_maps_editor_Editor_nativeShouldShowEditPlace(JNIEnv *, jclass)
{
  ::Framework * frm = g_framework->NativeFramework();
  if (!frm->HasPlacePageInfo())
    return static_cast<jboolean>(false);

  return g_framework->GetPlacePageInfo().ShouldShowEditPlace();
}

JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeShouldShowAddPlace(JNIEnv *, jclass)
{
  ::Framework * frm = g_framework->NativeFramework();
  if (!frm->HasPlacePageInfo())
    return static_cast<jboolean>(false);

  return g_framework->GetPlacePageInfo().ShouldShowAddPlace();
}

JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeShouldShowAddBusiness(JNIEnv *, jclass)
{
  ::Framework * frm = g_framework->NativeFramework();
  if (!frm->HasPlacePageInfo())
    return static_cast<jboolean>(false);

  return g_framework->GetPlacePageInfo().ShouldShowAddBusiness();
}

JNIEXPORT jintArray JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeGetEditableProperties(JNIEnv * env, jclass clazz)
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
Java_com_mapswithme_maps_editor_Editor_nativeIsAddressEditable(JNIEnv * env, jclass clazz)
{
  return g_editableMapObject.IsAddressEditable();
}

JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeIsNameEditable(JNIEnv * env, jclass clazz)
{
  return g_editableMapObject.IsNameEditable();
}

JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeIsPointType(JNIEnv * env, jclass clazz)
{
  return g_editableMapObject.IsPointType();
}

JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeIsBuilding(JNIEnv * env, jclass clazz)
{
  return g_editableMapObject.IsBuilding();
}

JNIEXPORT jobject JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeGetNamesDataSource(JNIEnv * env, jclass, jboolean needFakes)
{
  auto const namesDataSource = g_editableMapObject.GetNamesDataSource(needFakes);

  jobjectArray names = jni::ToJavaArray(env, g_localNameClazz, namesDataSource.names, ToJavaName);
  jsize const mandatoryNamesCount = static_cast<jsize>(namesDataSource.mandatoryNamesCount);

  return env->NewObject(g_namesDataSourceClassID, g_namesDataSourceConstructorID, names, mandatoryNamesCount);
}

JNIEXPORT jstring JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeGetDefaultName(JNIEnv * env, jclass)
{
  return jni::ToJavaString(env, g_editableMapObject.GetDefaultName());
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeEnableNamesAdvancedMode(JNIEnv *, jclass)
{
  g_editableMapObject.EnableNamesAdvancedMode();
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeSetNames(JNIEnv * env, jclass, jobjectArray names)
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
Java_com_mapswithme_maps_editor_Editor_nativeGetStreet(JNIEnv * env, jclass)
{
  return ToJavaStreet(env, g_editableMapObject.GetStreet());
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeSetStreet(JNIEnv * env, jclass, jobject street)
{
  g_editableMapObject.SetStreet({jni::ToNativeString(env, (jstring) env->GetObjectField(street, g_localStreetFieldDef)),
                                 jni::ToNativeString(env, (jstring) env->GetObjectField(street, g_localStreetFieldLoc))});
}
JNIEXPORT jobjectArray JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeGetNearbyStreets(JNIEnv * env, jclass clazz)
{
  return jni::ToJavaArray(env, g_localStreetClazz, g_editableMapObject.GetNearbyStreets(), ToJavaStreet);
}

JNIEXPORT jobjectArray JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeGetSupportedLanguages(JNIEnv * env, jclass clazz)
{
  using TLang = StringUtf8Multilang::Lang;
  //public Language(@NonNull String code, @NonNull String name)
  static jclass const langClass = jni::GetGlobalClassRef(env, "com/mapswithme/maps/editor/data/Language");
  static jmethodID const langCtor = jni::GetConstructorID(env, langClass, "(Ljava/lang/String;Ljava/lang/String;)V");

  return jni::ToJavaArray(env, langClass, StringUtf8Multilang::GetSupportedLanguages(),
                          [](JNIEnv * env, TLang const & lang)
                          {
                            jni::TScopedLocalRef const code(env, jni::ToJavaString(env, lang.m_code));
                            jni::TScopedLocalRef const name(env, jni::ToJavaString(env, lang.m_name));
                            return env->NewObject(langClass, langCtor, code.get(), name.get());
                          });
}

JNIEXPORT jstring JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeGetHouseNumber(JNIEnv * env, jclass)
{
  return jni::ToJavaString(env, g_editableMapObject.GetHouseNumber());
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeSetHouseNumber(JNIEnv * env, jclass, jstring houseNumber)
{
  g_editableMapObject.SetHouseNumber(jni::ToNativeString(env, houseNumber));
}

JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeHasSomethingToUpload(JNIEnv * env, jclass clazz)
{
  return Editor::Instance().HaveMapEditsOrNotesToUpload();
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeUploadChanges(JNIEnv * env, jclass clazz, jstring token, jstring secret,
    jstring appVersion, jstring appId)
{
  // TODO: Handle upload status in callback
  Editor::Instance().UploadChanges(jni::ToNativeString(env, token), jni::ToNativeString(env, secret),
      {{"created_by", "Organic Maps " OMIM_OS_NAME " " + jni::ToNativeString(env, appVersion)},
       {"bundle_id", jni::ToNativeString(env, appId)}}, nullptr);
}

JNIEXPORT jlongArray JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeGetStats(JNIEnv * env, jclass clazz)
{
  auto const stats = Editor::Instance().GetStats();
  jlongArray result = env->NewLongArray(3);
  jlong buf[] = {static_cast<jlong>(stats.m_edits.size()), static_cast<jlong>(stats.m_uploadedCount),
                 stats.m_lastUploadTimestamp};
  env->SetLongArrayRegion(result, 0, 3, buf);
  return result;
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeClearLocalEdits(JNIEnv * env, jclass clazz)
{
  Editor::Instance().ClearAllLocalEdits();
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeStartEdit(JNIEnv *, jclass)
{
  ::Framework * frm = g_framework->NativeFramework();
  if (!frm->HasPlacePageInfo())
    return;

  place_page::Info const & info = g_framework->GetPlacePageInfo();
  CHECK(frm->GetEditableMapObject(info.GetID(), g_editableMapObject), ("Invalid feature in the place page."));
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeCreateMapObject(JNIEnv * env, jclass,
                                                             jstring featureType)
{
  ::Framework * frm = g_framework->NativeFramework();
  auto const type = classif().GetTypeByReadableObjectName(jni::ToNativeString(env, featureType));
  CHECK(frm->CreateMapObject(frm->GetViewportCenter(), type, g_editableMapObject),
        ("Couldn't create mapobject, wrong coordinates of missing mwm"));
}

// static void nativeCreateNote(String text);
JNIEXPORT void JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeCreateNote(JNIEnv * env, jclass clazz, jstring text)
{
  g_framework->NativeFramework()->CreateNote(
      g_editableMapObject, osm::Editor::NoteProblemType::General, jni::ToNativeString(env, text));
}

// static void nativePlaceDoesNotExist(String comment);
JNIEXPORT void JNICALL
Java_com_mapswithme_maps_editor_Editor_nativePlaceDoesNotExist(JNIEnv * env, jclass clazz, jstring comment)
{
  g_framework->NativeFramework()->CreateNote(g_editableMapObject,
                                             osm::Editor::NoteProblemType::PlaceDoesNotExist,
                                             jni::ToNativeString(env, comment));
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeRollbackMapObject(JNIEnv * env, jclass clazz)
{
  g_framework->NativeFramework()->RollBackChanges(g_editableMapObject.GetID());
}

JNIEXPORT jobjectArray JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeGetAllCreatableFeatureTypes(JNIEnv * env, jclass clazz,
                                                                         jstring jLang)
{
  std::string const & lang = jni::ToNativeString(env, jLang);
  GetFeatureCategories().AddLanguage(lang);
  return jni::ToJavaStringArray(env, GetFeatureCategories().GetAllCreatableTypeNames());
}

JNIEXPORT jobjectArray JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeSearchCreatableFeatureTypes(JNIEnv * env, jclass clazz,
                                                                         jstring query,
                                                                         jstring jLang)
{
  std::string const & lang = jni::ToNativeString(env, jLang);
  GetFeatureCategories().AddLanguage(lang);
  return jni::ToJavaStringArray(env,
                                GetFeatureCategories().Search(jni::ToNativeString(env, query)));
}

JNIEXPORT jobjectArray JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeGetCuisines(JNIEnv * env, jclass clazz)
{
  osm::AllCuisines const & cuisines = osm::Cuisines::Instance().AllSupportedCuisines();
  std::vector<std::string> keys;
  keys.reserve(cuisines.size());
  for (TCuisine const & cuisine : cuisines)
    keys.push_back(cuisine.first);
  return jni::ToJavaStringArray(env, keys);
}

JNIEXPORT jobjectArray JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeGetSelectedCuisines(JNIEnv * env, jclass clazz)
{
  return jni::ToJavaStringArray(env, g_editableMapObject.GetCuisines());
}

JNIEXPORT jobjectArray JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeTranslateCuisines(JNIEnv * env, jclass clazz, jobjectArray jKeys)
{
  int const length = env->GetArrayLength(jKeys);
  std::vector<std::string> translations;
  translations.reserve(length);
  for (int i = 0; i < length; i++)
  {
    std::string const & key = jni::ToNativeString(env, (jstring) env->GetObjectArrayElement(jKeys, i));
    translations.push_back(osm::Cuisines::Instance().Translate(key));
  }
  return jni::ToJavaStringArray(env, translations);
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeSetSelectedCuisines(JNIEnv * env, jclass clazz, jobjectArray jKeys)
{
  int const length = env->GetArrayLength(jKeys);
  std::vector<std::string> cuisines;
  cuisines.reserve(length);
  for (int i = 0; i < length; i++)
    cuisines.push_back(jni::ToNativeString(env, (jstring) env->GetObjectArrayElement(jKeys, i)));
  g_editableMapObject.SetCuisines(cuisines);
}

JNIEXPORT jstring JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeGetFormattedCuisine(JNIEnv * env, jclass clazz)
{
  return jni::ToJavaString(env, g_editableMapObject.FormatCuisines());
}

JNIEXPORT jstring JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeGetMwmName(JNIEnv * env, jclass clazz)
{
  return jni::ToJavaString(env, g_editableMapObject.GetID().GetMwmName());
}

JNIEXPORT jlong JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeGetMwmVersion(JNIEnv * env, jclass clazz)
{
  return g_editableMapObject.GetID().GetMwmVersion();
}

// static boolean nativeIsHouseValid(String houseNumber);
JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeIsHouseValid(JNIEnv * env, jclass clazz, jstring houseNumber)
{
  return osm::EditableMapObject::ValidateHouseNumber(jni::ToNativeString(env, houseNumber));
}

// static boolean nativeIsLevelValid(String level);
JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeIsLevelValid(JNIEnv * env, jclass clazz, jstring level)
{
  return osm::EditableMapObject::ValidateBuildingLevels(jni::ToNativeString(env, level));
}

// static boolean nativeIsFlatValid(String flats)
JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeIsFlatValid(JNIEnv * env, jclass clazz, jstring flats)
{
  return osm::EditableMapObject::ValidateFlats(jni::ToNativeString(env, flats));
}

// static boolean nativeIsPostCodeValid(String zipCode)
JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeIsZipcodeValid(JNIEnv * env, jclass clazz, jstring zipCode)
{
  return osm::EditableMapObject::ValidatePostCode(jni::ToNativeString(env, zipCode));
}

// static boolean nativeIsPhoneValid(String phone)
JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeIsPhoneValid(JNIEnv * env, jclass clazz, jstring phone)
{
  return osm::EditableMapObject::ValidatePhoneList(jni::ToNativeString(env, phone));
}

// static boolean nativeIsWebsiteValid(String website)
JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeIsWebsiteValid(JNIEnv * env, jclass clazz, jstring website)
{
  return osm::EditableMapObject::ValidateWebsite(jni::ToNativeString(env, website));
}

// static boolean nativeIsFacebookPageValid(String facebookPage)
JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeIsFacebookPageValid(JNIEnv * env, jclass clazz, jstring facebookPage)
{
  return osm::EditableMapObject::ValidateFacebookPage(jni::ToNativeString(env, facebookPage));
}

// static boolean nativeIsInstagramPageValid(String instagramPage)
JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeIsInstagramPageValid(JNIEnv * env, jclass clazz, jstring instagramPage)
{
  return osm::EditableMapObject::ValidateInstagramPage(jni::ToNativeString(env, instagramPage));
}

// static boolean nativeIsTwitterPageValid(String twitterPage)
JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeIsTwitterPageValid(JNIEnv * env, jclass clazz, jstring twitterPage)
{
  return osm::EditableMapObject::ValidateTwitterPage(jni::ToNativeString(env, twitterPage));
}

// static boolean nativeIsVkPageValid(String vkPage)
JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeIsVkPageValid(JNIEnv * env, jclass clazz, jstring vkPage)
{
  return osm::EditableMapObject::ValidateVkPage(jni::ToNativeString(env, vkPage));
}

// static boolean nativeIsEmailValid(String email)
JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeIsEmailValid(JNIEnv * env, jclass clazz, jstring email)
{
  return osm::EditableMapObject::ValidateEmail(jni::ToNativeString(env, email));
}

JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeIsNameValid(JNIEnv * env, jclass clazz, jstring name)
{
  return osm::EditableMapObject::ValidateName(jni::ToNativeString(env, name));
}

JNIEXPORT jstring JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeGetCategory(JNIEnv * env, jclass clazz)
{
  auto types = g_editableMapObject.GetTypes();
  types.SortBySpec();
  return jni::ToJavaString(env, classif().GetReadableObjectName(*types.begin()));
}

// @FeatureStatus
// static native int nativeGetMapObjectStatus();
JNIEXPORT jint JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeGetMapObjectStatus(JNIEnv * env, jclass clazz)
{
  return static_cast<jint>(osm::Editor::Instance().GetFeatureStatus(g_editableMapObject.GetID()));
}

JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeIsMapObjectUploaded(JNIEnv * env, jclass clazz)
{
  return osm::Editor::Instance().IsFeatureUploaded(g_editableMapObject.GetID().m_mwmId, g_editableMapObject.GetID().m_index);
}

// static nativeMakeLocalizedName(String langCode, String name);
JNIEXPORT jobject JNICALL
Java_com_mapswithme_maps_editor_Editor_nativeMakeLocalizedName(JNIEnv * env, jclass clazz, jstring code, jstring name)
{
  osm::LocalizedName localizedName(jni::ToNativeString(env, code), jni::ToNativeString(env, name));
  return ToJavaName(env, localizedName);
}
} // extern "C"
