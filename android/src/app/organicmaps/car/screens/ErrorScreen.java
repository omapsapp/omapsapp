package app.organicmaps.car.screens;

import android.util.Log;

import androidx.annotation.NonNull;
import androidx.car.app.CarContext;
import androidx.car.app.Screen;
import androidx.car.app.model.LongMessageTemplate;
import androidx.car.app.model.Template;

import app.organicmaps.R;

public class ErrorScreen extends Screen
{
  private static final String TAG = ErrorScreen.class.getSimpleName();

  public ErrorScreen(@NonNull CarContext carContext)
  {
    super(carContext);
  }

  @NonNull
  @Override
  public Template onGetTemplate()
  {
    Log.d(TAG, "onGetTemplate");
    LongMessageTemplate.Builder builder = new LongMessageTemplate.Builder(getCarContext().getString(R.string.dialog_error_storage_message));
    builder.setTitle(getCarContext().getString(R.string.dialog_error_storage_title));

    return builder.build();
  }
}
