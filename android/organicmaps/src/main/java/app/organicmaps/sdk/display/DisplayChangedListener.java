package app.organicmaps.sdk.display;

import androidx.annotation.NonNull;

public interface DisplayChangedListener
{
  default void onDisplayChangedToDevice(@NonNull Runnable onTaskFinishedCallback) {}

  default void onDisplayChangedToCar(@NonNull Runnable onTaskFinishedCallback) {}
}
