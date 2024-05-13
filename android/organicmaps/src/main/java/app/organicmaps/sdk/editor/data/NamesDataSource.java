package app.organicmaps.sdk.editor.data;

import androidx.annotation.Keep;

/**
 * Class which contains array of localized names with following priority:
 * 1. Names for Mwm languages;
 * 2. User`s language name;
 * 3. Other names;
 * and mandatoryNamesCount - count of names which should be always shown.
 */
// Called from JNI.
@Keep
@SuppressWarnings("unused")
public class NamesDataSource
{
  private final LocalizedName[] mNames;
  private final int mMandatoryNamesCount;

  public NamesDataSource(LocalizedName[] names, int mandatoryNamesCount)
  {
    this.mNames = names;
    this.mMandatoryNamesCount = mandatoryNamesCount;
  }

  public LocalizedName[] getNames()
  {
    return mNames;
  }

  public int getMandatoryNamesCount()
  {
    return mMandatoryNamesCount;
  }
}
