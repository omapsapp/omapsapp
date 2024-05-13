package app.organicmaps.routing;

import android.content.Context;
import android.content.res.Resources;
import android.util.Pair;

import androidx.annotation.NonNull;
import androidx.annotation.StringRes;

import app.organicmaps.MwmApplication;
import app.organicmaps.R;
import app.organicmaps.sdk.location.LocationHelper;

import java.util.ArrayList;
import java.util.List;

public class ResultCodesHelper
{
  // Codes correspond to native routing::RouterResultCode in routing/routing_callbacks.hpp
  static final int NO_ERROR = 0;
  static final int CANCELLED = 1;
  static final int NO_POSITION = 2;
  private static final int INCONSISTENT_MWM_ROUTE = 3;
  private static final int ROUTING_FILE_NOT_EXIST = 4;
  private static final int START_POINT_NOT_FOUND = 5;
  private static final int END_POINT_NOT_FOUND = 6;
  private static final int DIFFERENT_MWM = 7;
  private static final int ROUTE_NOT_FOUND = 8;
  private static final int NEED_MORE_MAPS = 9;
  private static final int INTERNAL_ERROR = 10;
  private static final int FILE_TOO_OLD = 11;
  private static final int INTERMEDIATE_POINT_NOT_FOUND = 12;
  private static final int TRANSIT_ROUTE_NOT_FOUND_NO_NETWORK = 13;
  private static final int TRANSIT_ROUTE_NOT_FOUND_TOO_LONG_PEDESTRIAN = 14;
  private static final int ROUTE_NOT_FOUND_REDRESS_ROUTE_ERROR = 15;
  static final int HAS_WARNINGS = 16;

  @NonNull
  public static ResourcesHolder getDialogTitleSubtitle(@NonNull Context context,
                                                int errorCode, int missingCount)
  {
    Resources resources = MwmApplication.from(context).getResources();
    int titleRes = 0;
    List<String> messages = new ArrayList<>();
    @StringRes
    int cancelBtnResId = android.R.string.cancel;
    switch (errorCode)
    {
    case NO_POSITION:
      if (!LocationHelper.from(context).isActive())
      {
        titleRes = R.string.dialog_routing_location_turn_on;
        messages.add(resources.getString(R.string.dialog_routing_location_unknown_turn_on));
      }
      else
      {
        titleRes = R.string.dialog_routing_check_gps;
        messages.add(resources.getString(R.string.dialog_routing_error_location_not_found));
        messages.add(resources.getString(R.string.dialog_routing_location_turn_wifi));
      }
      break;
    case INCONSISTENT_MWM_ROUTE:
    case ROUTING_FILE_NOT_EXIST:
      titleRes = R.string.routing_download_maps_along;
      messages.add(resources.getString(R.string.routing_requires_all_map));
      break;
    case START_POINT_NOT_FOUND:
      titleRes = R.string.dialog_routing_change_start;
      messages.add(resources.getString(R.string.dialog_routing_start_not_determined));
      messages.add(resources.getString(R.string.dialog_routing_select_closer_start));
      break;
    case END_POINT_NOT_FOUND:
      titleRes = R.string.dialog_routing_change_end;
      messages.add(resources.getString(R.string.dialog_routing_end_not_determined));
      messages.add(resources.getString(R.string.dialog_routing_select_closer_end));
      break;
    case INTERMEDIATE_POINT_NOT_FOUND:
      titleRes = R.string.dialog_routing_change_intermediate;
      messages.add(resources.getString(R.string.dialog_routing_intermediate_not_determined));
      break;
    case DIFFERENT_MWM:
      messages.add(resources.getString(R.string.routing_failed_cross_mwm_building));
      break;
    case FILE_TOO_OLD:
      titleRes = R.string.downloader_update_maps;
      messages.add(resources.getString(R.string.downloader_mwm_migration_dialog));
      break;
    case TRANSIT_ROUTE_NOT_FOUND_NO_NETWORK:
      messages.add(resources.getString(R.string.transit_not_found));
      break;
    case TRANSIT_ROUTE_NOT_FOUND_TOO_LONG_PEDESTRIAN:
      titleRes = R.string.dialog_pedestrian_route_is_long_header;
      messages.add(resources.getString(R.string.dialog_pedestrian_route_is_long_message));
      cancelBtnResId = R.string.ok;
      break;
    case ROUTE_NOT_FOUND:
    case ROUTE_NOT_FOUND_REDRESS_ROUTE_ERROR:
      if (missingCount == 0)
      {
        titleRes = R.string.dialog_routing_unable_locate_route;
        messages.add(resources.getString(R.string.dialog_routing_cant_build_route));
        messages.add(resources.getString(R.string.dialog_routing_change_start_or_end));
      }
      else
      {
        titleRes = R.string.routing_download_maps_along;
        messages.add(resources.getString(R.string.routing_requires_all_map));
      }
      break;
    case INTERNAL_ERROR:
      titleRes = R.string.dialog_routing_system_error;
      messages.add(resources.getString(R.string.dialog_routing_application_error));
      messages.add(resources.getString(R.string.dialog_routing_try_again));
      break;
    case NEED_MORE_MAPS:
      titleRes = R.string.dialog_routing_download_and_build_cross_route;
      messages.add(resources.getString(R.string.dialog_routing_download_cross_route));
      break;
    }

    StringBuilder builder = new StringBuilder();
    for (String messagePart : messages)
    {
      if (builder.length() > 0)
        builder.append("\n\n");

      builder.append(messagePart);
    }

    return new ResourcesHolder(
        new Pair<>(titleRes == 0 ? "" : resources.getString(titleRes), builder.toString()),
        cancelBtnResId);
  }

  public static boolean isDownloadable(int resultCode, int missingCount)
  {
    if (missingCount <= 0)
      return false;

    return switch (resultCode)
    {
      case INCONSISTENT_MWM_ROUTE,
          ROUTE_NOT_FOUND_REDRESS_ROUTE_ERROR,
          ROUTING_FILE_NOT_EXIST,
          NEED_MORE_MAPS,
          ROUTE_NOT_FOUND,
          FILE_TOO_OLD ->
          true;
      default -> false;
    };
  }

  public static boolean isMoreMapsNeeded(int resultCode)
  {
    return resultCode == NEED_MORE_MAPS;
  }

  public static class ResourcesHolder
  {
    @NonNull
    private final Pair<String, String> mTitleMessage;

    private final int mCancelBtnResId;

    private ResourcesHolder(@NonNull Pair<String, String> titleMessage, int cancelBtnResId)
    {
      mTitleMessage = titleMessage;
      mCancelBtnResId = cancelBtnResId;
    }

    @NonNull
    public Pair<String, String> getTitleMessage()
    {
      return mTitleMessage;
    }

    public int getCancelBtnResId()
    {
      return mCancelBtnResId;
    }
  }
}
