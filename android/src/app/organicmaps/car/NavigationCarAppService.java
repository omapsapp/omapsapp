package app.organicmaps.car;

import androidx.annotation.NonNull;
import androidx.car.app.CarAppService;
import androidx.car.app.Session;
import androidx.car.app.validation.HostValidator;

import app.organicmaps.BuildConfig;

public final class NavigationCarAppService extends CarAppService
{
  @NonNull
  @Override
  public HostValidator createHostValidator()
  {
    if (BuildConfig.DEBUG)
      return HostValidator.ALLOW_ALL_HOSTS_VALIDATOR;

    return new HostValidator.Builder(getApplicationContext())
        .addAllowedHosts(androidx.car.app.R.array.hosts_allowlist_sample)
        .build();
  }

  @NonNull
  @Override
  public Session onCreateSession()
  {
    return new NavigationSession();
  }
}
