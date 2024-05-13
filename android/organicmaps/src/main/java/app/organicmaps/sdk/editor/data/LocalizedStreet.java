package app.organicmaps.sdk.editor.data;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;

// Called from JNI.
@Keep
@SuppressWarnings("unused")
public class LocalizedStreet
{
  public final String defaultName;
  public final String localizedName;

  public LocalizedStreet(@NonNull String defaultName, @NonNull String localizedName)
  {
    this.defaultName = defaultName;
    this.localizedName = localizedName;
  }
}
