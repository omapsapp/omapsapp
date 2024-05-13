package app.organicmaps.sdk.maplayer.subway;

import android.content.Context;
import android.widget.Toast;

import androidx.annotation.NonNull;

enum TransitSchemeState
{
  DISABLED,
  ENABLED,
  NO_DATA
      {
        @Override
        public void activate(@NonNull Context context)
        {
//          Toast.makeText(context, R.string.subway_data_unavailable, Toast.LENGTH_SHORT).show();
        }
      };

  void activate(@NonNull Context context)
  {
    /* Do nothing by default */
  }
}
