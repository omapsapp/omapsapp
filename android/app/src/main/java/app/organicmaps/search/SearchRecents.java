package app.organicmaps.search;

import android.content.Context;
import android.text.TextUtils;
import android.util.Pair;

import androidx.annotation.NonNull;

import app.organicmaps.util.Language;

import java.util.ArrayList;
import java.util.List;

public final class SearchRecents
{
  private static final List<String> sRecents = new ArrayList<>();

  private SearchRecents() {}

  public static void refresh()
  {
    sRecents.clear();
    nativeGetList(sRecents);
  }

  public static int getSize()
  {
    return sRecents.size();
  }

  public static String get(int position)
  {
    return sRecents.get(position);
  }

  public static boolean add(@NonNull String query, @NonNull Context context)
  {
    if (TextUtils.isEmpty(query))
      return false;

    nativeAdd(Language.getKeyboardLocale(context), query);
    refresh();
    return true;
  }

  public static void clear()
  {
    nativeClear();
    sRecents.clear();
  }

  private static native void nativeGetList(List<String> result);
  private static native void nativeAdd(String locale, String query);
  private static native void nativeClear();
}
