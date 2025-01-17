package app.organicmaps.sdk;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;

import app.organicmaps.widget.placepage.PlacePageData;

public interface PlacePageActivationListener
{
  // Called from JNI.
  @Keep
  @SuppressWarnings("unused")
  void onPlacePageActivated(@NonNull PlacePageData data);

  // Called from JNI
  @Keep
  @SuppressWarnings("unused")
  void onPlacePageDeactivated();

  // Called from JNI
  @Keep
  @SuppressWarnings("unused")
  void onSwitchFullScreenMode();
}
