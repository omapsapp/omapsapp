package app.organicmaps.sdk;

import androidx.annotation.NonNull;

public enum ChoosePositionMode
{
  None(0),
  Editor(1),
  Api(2);

  ChoosePositionMode(int mode)
  {
    this.mode = mode;
  }

  /**
   * @param isBusiness    selection area will be bounded by building borders, if its true (eg. true for businesses in buildings).
   * @param applyPosition if true, map will be animated to currently selected object.
   */
  public static void set(@NonNull ChoosePositionMode mode, boolean isBusiness, boolean applyPosition)
  {
    nativeSet(mode.mode, isBusiness, applyPosition);
  }

  public static ChoosePositionMode get()
  {
    return ChoosePositionMode.values()[nativeGet()];
  }

  private final int mode;


  private static native void nativeSet(int mode, boolean isBusiness,
                                                         boolean applyPosition);

  private static native int nativeGet();
}
