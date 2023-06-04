package app.organicmaps.search;

import androidx.annotation.NonNull;

class DisplayedCategories
{
  @NonNull
  public static String[] getKeys()
  {
    return nativeGetKeys();
  }

  @NonNull
  private static native String[] nativeGetKeys();
}
