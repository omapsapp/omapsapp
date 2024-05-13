package app.organicmaps.sdk.content;

import androidx.annotation.NonNull;

public interface DataSource<D>
{
  @NonNull
  D getData();
}
