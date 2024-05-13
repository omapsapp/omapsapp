package app.organicmaps.sdk.maplayer.isolines;

import android.content.Context;
import android.view.View;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

public enum IsolinesState
{
  DISABLED,
  ENABLED,
  EXPIREDDATA
      {
        @Override
        public void activate(@NonNull Context context, @Nullable View view, @Nullable View viewAbove)
        {
//          if (view != null)
//            Utils.showSnackbar(context, view, viewAbove, R.string.isolines_activation_error_dialog);
//          else
//            Toast.makeText(context, R.string.isolines_activation_error_dialog, Toast.LENGTH_SHORT)
//                 .show();
        }
      },
  NODATA
      {
        @Override
        public void activate(@NonNull Context context, @Nullable View view, @Nullable View viewAbove)
        {
//          if (view != null)
//            Utils.showSnackbar(context, view, viewAbove, R.string.isolines_location_error_dialog);
//          else
//            Toast.makeText(context, R.string.isolines_location_error_dialog, Toast.LENGTH_SHORT)
//                 .show();
        }
      };

  public void activate(@NonNull Context context, @Nullable View viewAbove, @Nullable View view)
  {
    /* Do nothing by default */
  }
}
