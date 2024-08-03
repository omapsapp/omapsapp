package app.organicmaps.widget.placepage;

import android.content.Context;
import android.location.Location;
import android.os.Bundle;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.TextUtils;
import android.text.style.ForegroundColorSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.TextView;

import androidx.annotation.IdRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.widget.Toolbar;
import androidx.core.content.ContextCompat;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.lifecycle.Observer;
import androidx.lifecycle.ViewModelProvider;
import app.organicmaps.Framework;
import app.organicmaps.MwmActivity;
import app.organicmaps.MwmApplication;
import app.organicmaps.R;
import app.organicmaps.bookmarks.data.DistanceAndAzimut;
import app.organicmaps.bookmarks.data.MapObject;
import app.organicmaps.bookmarks.data.Metadata;
import app.organicmaps.downloader.CountryItem;
import app.organicmaps.downloader.DownloaderStatusIcon;
import app.organicmaps.downloader.MapManager;
import app.organicmaps.editor.Editor;
import app.organicmaps.location.LocationHelper;
import app.organicmaps.location.LocationListener;
import app.organicmaps.location.SensorHelper;
import app.organicmaps.location.SensorListener;
import app.organicmaps.routing.RoutingController;
import app.organicmaps.util.SharingUtils;
import app.organicmaps.util.StringUtils;
import app.organicmaps.util.UiUtils;
import app.organicmaps.util.Utils;
import app.organicmaps.util.concurrency.UiThread;
import app.organicmaps.widget.ArrowView;
import app.organicmaps.widget.placepage.sections.PlacePageBookmarkFragment;
import app.organicmaps.widget.placepage.sections.PlacePageLinksFragment;
import app.organicmaps.widget.placepage.sections.PlacePageOpeningHoursFragment;
import app.organicmaps.widget.placepage.sections.PlacePagePhoneFragment;
import app.organicmaps.widget.placepage.sections.PlacePageWikipediaFragment;
import com.google.android.material.button.MaterialButton;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import static android.view.View.GONE;
import static android.view.View.VISIBLE;

public class PlacePageView extends Fragment implements View.OnClickListener,
                                                       View.OnLongClickListener,
                                                       LocationListener,
                                                       SensorListener,
                                                       Observer<MapObject>

{
  private static final String PREF_COORDINATES_FORMAT = "coordinates_format";
  private static final String BOOKMARK_FRAGMENT_TAG = "BOOKMARK_FRAGMENT_TAG";
  private static final String WIKIPEDIA_FRAGMENT_TAG = "WIKIPEDIA_FRAGMENT_TAG";
  private static final String PHONE_FRAGMENT_TAG = "PHONE_FRAGMENT_TAG";
  private static final String OPENING_HOURS_FRAGMENT_TAG = "OPENING_HOURS_FRAGMENT_TAG";
  private static final String LINKS_FRAGMENT_TAG = "LINKS_FRAGMENT_TAG";

  private static final List<CoordinatesFormat> visibleCoordsFormat =
      Arrays.asList(CoordinatesFormat.LatLonDMS,
                    CoordinatesFormat.LatLonDecimal,
                    CoordinatesFormat.OLCFull,
                    CoordinatesFormat.UTM,
                    CoordinatesFormat.MGRS,
                    CoordinatesFormat.OSMLink);
  private View mFrame;
  // Preview.
  private ViewGroup mPreview;
  private Toolbar mToolbar;
  private TextView mTvTitle;
  private TextView mTvSecondaryTitle;
  private TextView mTvSubtitle;
  private ArrowView mAvDirection;
  private TextView mTvDistance;
  private TextView mTvAddress;
  // Details.
  private TextView mTvLatlon;
  private View mWifi;
  private TextView mTvWiFi;
  private View mOperator;
  private TextView mTvOperator;
  private View mLevel;
  private TextView mTvLevel;
  private View mAtm;
  private TextView mTvAtm;
  private View mCapacity;
  private TextView mTvCapacity;
  private View mWheelchair;
  private TextView mTvWheelchair;
  private View mDriveThrough;
  private TextView mTvDriveThrough;
  private View mCuisine;
  private TextView mTvCuisine;
  private View mEntrance;
  private TextView mTvEntrance;
  private View mEditPlace;
  private View mAddOrganisation;
  private View mAddPlace;
  private View mEditTopSpace;
  private View mCollectionTimes;
  private TextView mTvCollectionTimes;

  // Data
  private CoordinatesFormat mCoordsFormat = CoordinatesFormat.LatLonDecimal;
  // Downloader`s stuff
  private DownloaderStatusIcon mDownloaderIcon;
  private TextView mDownloaderInfo;
  private int mStorageCallbackSlot;
  @Nullable
  private CountryItem mCurrentCountry;
  private final MapManager.StorageCallback mStorageCallback = new MapManager.StorageCallback()
  {
    @Override
    public void onStatusChanged(List<MapManager.StorageCallbackData> data)
    {
      if (mCurrentCountry == null)
        return;

      for (MapManager.StorageCallbackData item : data)
        if (mCurrentCountry.id.equals(item.countryId))
        {
          updateDownloader();
          return;
        }
    }

    @Override
    public void onProgress(String countryId, long localSize, long remoteSize)
    {
      if (mCurrentCountry != null && mCurrentCountry.id.equals(countryId))
        updateDownloader();
    }
  };
  private PlacePageViewListener mPlacePageViewListener;

  private PlacePageViewModel mViewModel;
  private MapObject mMapObject;

  private static void refreshMetadataOrHide(@Nullable String metadata, @NonNull View metaLayout, @NonNull TextView metaTv)
  {
    if (!TextUtils.isEmpty(metadata))
    {
      metaLayout.setVisibility(VISIBLE);
      metaTv.setText(metadata);
    }
    else
      metaLayout.setVisibility(GONE);
  }

  private static boolean isInvalidDownloaderStatus(int status)
  {
    return (status != CountryItem.STATUS_DOWNLOADABLE &&
            status != CountryItem.STATUS_ENQUEUED &&
            status != CountryItem.STATUS_FAILED &&
            status != CountryItem.STATUS_PARTLY &&
            status != CountryItem.STATUS_PROGRESS &&
            status != CountryItem.STATUS_APPLYING);
  }

  @Nullable
  @Override
  public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState)
  {
    mViewModel = new ViewModelProvider(requireActivity()).get(PlacePageViewModel.class);
    return inflater.inflate(R.layout.place_page, container, false);
  }

  @Override
  public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);
    mCoordsFormat = CoordinatesFormat.fromId(
        MwmApplication.prefs(requireContext()).getInt(
            PREF_COORDINATES_FORMAT, CoordinatesFormat.LatLonDecimal.getId()));

    Fragment parentFragment = getParentFragment();
    mPlacePageViewListener = (PlacePageViewListener) parentFragment;

    mFrame = view;
    mFrame.setOnClickListener((v) -> mPlacePageViewListener.onPlacePageRequestToggleState());

    mPreview = mFrame.findViewById(R.id.pp__preview);
    mFrame.addOnLayoutChangeListener((v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
      final int oldHeight = oldBottom - oldTop;
      final int newHeight = bottom - top;
      if (oldHeight != newHeight)
        mPlacePageViewListener.onPlacePageContentChanged(mPreview.getHeight(), newHeight);
    });

    mTvTitle = mPreview.findViewById(R.id.tv__title);
    mTvTitle.setOnLongClickListener(this);
    mTvTitle.setOnClickListener(this);
    mTvSecondaryTitle = mPreview.findViewById(R.id.tv__secondary_title);
    mTvSecondaryTitle.setOnLongClickListener(this);
    mTvSecondaryTitle.setOnClickListener(this);
    mToolbar = mFrame.findViewById(R.id.toolbar);
    mTvSubtitle = mPreview.findViewById(R.id.tv__subtitle);

    View directionFrame = mPreview.findViewById(R.id.direction_frame);
    mTvDistance = mPreview.findViewById(R.id.tv__straight_distance);
    mAvDirection = mPreview.findViewById(R.id.av__direction);
    UiUtils.hide(mTvDistance);
    UiUtils.hide(mAvDirection);
    directionFrame.setOnClickListener(this);

    mTvAddress = mPreview.findViewById(R.id.tv__address);
    mTvAddress.setOnLongClickListener(this);
    mTvAddress.setOnClickListener(this);

    MaterialButton shareButton = mPreview.findViewById(R.id.share_button);
    shareButton.setOnClickListener((v) -> SharingUtils.shareMapObject(requireContext(), mMapObject));

    final MaterialButton closeButton = mPreview.findViewById(R.id.close_button);
    closeButton.setOnClickListener((v) -> mPlacePageViewListener.onPlacePageRequestClose());

    RelativeLayout address = mFrame.findViewById(R.id.ll__place_name);

    LinearLayout latlon = mFrame.findViewById(R.id.ll__place_latlon);
    latlon.setOnClickListener(this);
    mTvLatlon = mFrame.findViewById(R.id.tv__place_latlon);
    mWifi = mFrame.findViewById(R.id.ll__place_wifi);
    mTvWiFi = mFrame.findViewById(R.id.tv__place_wifi);
    mOperator = mFrame.findViewById(R.id.ll__place_operator);
    mOperator.setOnClickListener(this);
    mTvOperator = mFrame.findViewById(R.id.tv__place_operator);
    mLevel = mFrame.findViewById(R.id.ll__place_level);
    mTvLevel = mFrame.findViewById(R.id.tv__place_level);
    mAtm = mFrame.findViewById(R.id.ll__place_atm);
    mTvAtm = mFrame.findViewById(R.id.tv__place_atm);
    mCapacity = mFrame.findViewById(R.id.ll__place_capacity);
    mTvCapacity = mFrame.findViewById(R.id.tv__place_capacity);
    mWheelchair = mFrame.findViewById(R.id.ll__place_wheelchair);
    mTvWheelchair = mFrame.findViewById(R.id.tv__place_wheelchair);
    mDriveThrough = mFrame.findViewById(R.id.ll__place_drive_through);
    mTvDriveThrough = mFrame.findViewById(R.id.tv__place_drive_through);
    mCuisine = mFrame.findViewById(R.id.ll__place_cuisine);
    mTvCuisine = mFrame.findViewById(R.id.tv__place_cuisine);
    mEntrance = mFrame.findViewById(R.id.ll__place_entrance);
    mTvEntrance = mEntrance.findViewById(R.id.tv__place_entrance);
    mEditPlace = mFrame.findViewById(R.id.ll__place_editor);
    mEditPlace.setOnClickListener(this);
    mAddOrganisation = mFrame.findViewById(R.id.ll__add_organisation);
    mAddOrganisation.setOnClickListener(this);
    mAddPlace = mFrame.findViewById(R.id.ll__place_add);
    mAddPlace.setOnClickListener(this);
    mEditTopSpace = mFrame.findViewById(R.id.edit_top_space);
    latlon.setOnLongClickListener(this);
    address.setOnLongClickListener(this);
    mOperator.setOnLongClickListener(this);
    mLevel.setOnLongClickListener(this);
    mAtm.setOnLongClickListener(this);
    mCapacity.setOnLongClickListener(this);
    mWheelchair.setOnLongClickListener(this);
    mDriveThrough.setOnLongClickListener(this);
    mCollectionTimes = mFrame.findViewById(R.id.ll__place_collection_times);
    mTvCollectionTimes = mFrame.findViewById(R.id.tv__place_collection_times);
    mCollectionTimes.setOnLongClickListener(this);

    mDownloaderIcon = new DownloaderStatusIcon(mPreview.findViewById(R.id.downloader_status_frame));

    mDownloaderInfo = mPreview.findViewById(R.id.tv__downloader_details);
  }

  @Override
  public void onStart()
  {
    super.onStart();
    mViewModel.getMapObject().observe(requireActivity(), this);
    LocationHelper.from(requireContext()).addListener(this);
    SensorHelper.from(requireContext()).addListener(this);
  }

  @Override
  public void onStop()
  {
    super.onStop();
    mViewModel.getMapObject().removeObserver(this);
    LocationHelper.from(requireContext()).removeListener(this);
    SensorHelper.from(requireContext()).removeListener(this);
    detachCountry();
  }

  private void setCurrentCountry()
  {
    if (mCurrentCountry != null)
      return;
    String country = MapManager.nativeGetSelectedCountry();
    if (country != null && !RoutingController.get().isNavigating())
      attachCountry(country);
  }

  private void refreshViews()
  {
    refreshPreview();
    refreshDetails();
    final Location loc = LocationHelper.from(requireContext()).getSavedLocation();
    if (mMapObject.isMyPosition())
      refreshMyPosition(loc);
    else
      refreshDistanceToObject(loc);
  }

  private <T extends Fragment> void updateViewFragment(Class<T> controllerClass, String fragmentTag, @IdRes int containerId, boolean enabled)
  {
    final FragmentManager fm = getChildFragmentManager();
    final Fragment fragment = fm.findFragmentByTag(fragmentTag);
    if (enabled && fragment == null)
    {
      fm.beginTransaction()
        .setReorderingAllowed(true)
        .add(containerId, controllerClass, null, fragmentTag)
        .commit();
    }
    else if (!enabled && fragment != null)
    {
      fm.beginTransaction()
        .setReorderingAllowed(true)
        .remove(fragment)
        .commit();
    }
  }

  private void updateLinksView()
  {
    updateViewFragment(PlacePageLinksFragment.class, LINKS_FRAGMENT_TAG, R.id.place_page_links_fragment, true);
  }

  private void updateOpeningHoursView()
  {
    final String ohStr = mMapObject.getMetadata(Metadata.MetadataType.FMD_OPEN_HOURS);
    updateViewFragment(PlacePageOpeningHoursFragment.class, OPENING_HOURS_FRAGMENT_TAG,
        R.id.place_page_opening_hours_fragment, !TextUtils.isEmpty(ohStr));
  }

  private void updatePhoneView()
  {
    updateViewFragment(PlacePagePhoneFragment.class, PHONE_FRAGMENT_TAG, R.id.place_page_phone_fragment, mMapObject.hasPhoneNumber());
  }

  private void updateBookmarkView()
  {
    updateViewFragment(PlacePageBookmarkFragment.class, BOOKMARK_FRAGMENT_TAG, R.id.place_page_bookmark_fragment,
        mMapObject.isBookmark());
  }

  private boolean hasWikipediaEntry()
  {
    final String wikipediaLink = mMapObject.getMetadata(Metadata.MetadataType.FMD_WIKIPEDIA);
    final String description = mMapObject.getDescription();
    return !TextUtils.isEmpty(wikipediaLink) || !TextUtils.isEmpty(description);
  }

  private void updateWikipediaView()
  {
    updateViewFragment(PlacePageWikipediaFragment.class, WIKIPEDIA_FRAGMENT_TAG, R.id.place_page_wikipedia_fragment, hasWikipediaEntry());
  }

  private void setTextAndColorizeSubtitle()
  {
    String text = mMapObject.getSubtitle();
    UiUtils.setTextAndHideIfEmpty(mTvSubtitle, text);
    if (!TextUtils.isEmpty(text))
    {
      SpannableStringBuilder sb = new SpannableStringBuilder(text);
      int start = text.indexOf("★");
      int end = text.lastIndexOf("★") + 1;
      if (start > -1)
      {
        sb.setSpan(new ForegroundColorSpan(ContextCompat.getColor(requireContext(), R.color.base_yellow)),
                   start, end, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
      }
      mTvSubtitle.setText(sb);
    }
  }

  private void refreshPreview()
  {
    UiUtils.setTextAndHideIfEmpty(mTvTitle, mMapObject.getTitle());
    UiUtils.setTextAndHideIfEmpty(mTvSecondaryTitle, mMapObject.getSecondaryTitle());
    if (mToolbar != null)
      mToolbar.setTitle(mMapObject.getTitle());
    setTextAndColorizeSubtitle();
    UiUtils.setTextAndHideIfEmpty(mTvAddress, mMapObject.getAddress());
  }

  private void refreshDetails()
  {
    refreshLatLon();

    refreshMetadataOrHide(mMapObject.getMetadata(Metadata.MetadataType.FMD_OPERATOR), mOperator, mTvOperator);
    /// @todo I don't like it when we take all data from mapObject, but for cuisines, we should
    /// go into JNI Framework and rely on some "active object".
    refreshMetadataOrHide(Framework.nativeGetActiveObjectFormattedCuisine(), mCuisine, mTvCuisine);
    refreshWiFi();
    refreshMetadataOrHide(mMapObject.getMetadata(Metadata.MetadataType.FMD_FLATS), mEntrance, mTvEntrance);
    final String level = Utils.getLocalizedLevel(getContext(), mMapObject.getMetadata(Metadata.MetadataType.FMD_LEVEL));
    refreshMetadataOrHide(level, mLevel, mTvLevel);

    final String collectionTimes = mMapObject.getMetadata(Metadata.MetadataType.FMD_COLLECTION_TIMES);
    refreshMetadataOrHide(collectionTimes, mCollectionTimes, mTvCollectionTimes);

    final String cap = mMapObject.getMetadata(Metadata.MetadataType.FMD_CAPACITY);
    refreshMetadataOrHide(!TextUtils.isEmpty(cap) ? getString(R.string.capacity, cap) : "", mCapacity, mTvCapacity);

    refreshMetadataOrHide(mMapObject.hasAtm() ? getString(R.string.type_amenity_atm) : "", mAtm, mTvAtm);

    final String wheelchair = Utils.getLocalizedFeatureType(getContext(), mMapObject.getMetadata(Metadata.MetadataType.FMD_WHEELCHAIR));
    refreshMetadataOrHide(wheelchair, mWheelchair, mTvWheelchair);

    final String driveThrough = mMapObject.getMetadata(Metadata.MetadataType.FMD_DRIVE_THROUGH);
    if (driveThrough.equals("yes"))
    {
      refreshMetadataOrHide(getString(R.string.drive_through), mDriveThrough, mTvDriveThrough);
    }

//    showTaxiOffer(mapObject);

    if (RoutingController.get().isNavigating() || RoutingController.get().isPlanning())
    {
      UiUtils.hide(mEditPlace, mAddOrganisation, mAddPlace, mEditTopSpace);
    }
    else
    {
      UiUtils.showIf(Editor.nativeShouldShowEditPlace(), mEditPlace);
      UiUtils.showIf(Editor.nativeShouldShowAddBusiness(), mAddOrganisation);
      UiUtils.showIf(Editor.nativeShouldShowAddPlace(), mAddPlace);
      UiUtils.showIf(UiUtils.isVisible(mEditPlace)
                     || UiUtils.isVisible(mAddOrganisation)
                     || UiUtils.isVisible(mAddPlace), mEditTopSpace);
    }
    updateLinksView();
    updateOpeningHoursView();
    updateWikipediaView();
    updateBookmarkView();
    updatePhoneView();
  }

  private void refreshWiFi()
  {
    final String inet = mMapObject.getMetadata(Metadata.MetadataType.FMD_INTERNET);
    if (!TextUtils.isEmpty(inet))
    {
      mWifi.setVisibility(VISIBLE);
      /// @todo Better (but harder) to wrap C++ osm::Internet into Java, instead of comparing with "no".
      mTvWiFi.setText(TextUtils.equals(inet, "no") ? R.string.no_available : R.string.yes_available);
    }
    else
      mWifi.setVisibility(GONE);
  }

  private void refreshMyPosition(Location l)
  {
    UiUtils.hide(mTvDistance);
    UiUtils.hide(mAvDirection);

    if (l == null)
      return;

    final StringBuilder builder = new StringBuilder();
    if (l.hasAltitude())
      builder.append("▲").append(Framework.nativeFormatAltitude(l.getAltitude()));
    if (l.hasSpeed())
      builder.append("   ").append(Framework.nativeFormatSpeed(l.getSpeed()));

    UiUtils.setTextAndHideIfEmpty(mTvSubtitle, builder.toString());

    mMapObject.setLat(l.getLatitude());
    mMapObject.setLon(l.getLongitude());
    refreshLatLon();
  }

  private void refreshDistanceToObject(Location l)
  {
    UiUtils.showIf(l != null, mTvDistance);
    if (l == null)
      return;

    double lat = mMapObject.getLat();
    double lon = mMapObject.getLon();
    DistanceAndAzimut distanceAndAzimuth =
        Framework.nativeGetDistanceAndAzimuthFromLatLon(lat, lon, l.getLatitude(), l.getLongitude(), 0.0);
    mTvDistance.setText(distanceAndAzimuth.getDistance().toString(requireContext()));
  }

  private void refreshLatLon()
  {
    final double lat = mMapObject.getLat();
    final double lon = mMapObject.getLon();
    String latLon = Framework.nativeFormatLatLon(lat, lon, mCoordsFormat.getId());
    if (latLon == null) // Some coordinates couldn't be converted to UTM and MGRS
      latLon = "N/A";

    if (mCoordsFormat.showLabel())
      mTvLatlon.setText(mCoordsFormat.getLabel() + ": " + latLon);
    else
      mTvLatlon.setText(latLon);
  }

  private void addOrganisation()
  {
    ((MwmActivity) requireActivity()).showPositionChooserForEditor(true, false);
  }

  private void addPlace()
  {
    ((MwmActivity) requireActivity()).showPositionChooserForEditor(false, true);
  }

  /// @todo
  /// - Why ll__place_editor and ll__place_latlon check if (mMapObject == null)

  @Override
  public void onClick(View v)
  {
    final Context context = requireContext();
    final int id = v.getId();
    if (id == R.id.tv__title || id == R.id.tv__secondary_title || id == R.id.tv__address)
    {
      // A workaround to make single taps toggle the bottom sheet.
      mPlacePageViewListener.onPlacePageRequestToggleState();
    }
    else if (id == R.id.ll__place_editor)
      ((MwmActivity) requireActivity()).showEditor();
    else if (id == R.id.ll__add_organisation)
      addOrganisation();
    else if (id == R.id.ll__place_add)
      addPlace();
    else if (id == R.id.ll__place_latlon)
    {
      final int formatIndex = visibleCoordsFormat.indexOf(mCoordsFormat);
      mCoordsFormat = visibleCoordsFormat.get((formatIndex + 1) % visibleCoordsFormat.size());
      MwmApplication.prefs(context)
                    .edit()
                    .putInt(PREF_COORDINATES_FORMAT, mCoordsFormat.getId())
                    .apply();
      refreshLatLon();
    }
    else if (id == R.id.direction_frame)
      showBigDirection();
  }

  private void showBigDirection()
  {
    final FragmentManager fragmentManager = requireActivity().getSupportFragmentManager();
    final DirectionFragment fragment = (DirectionFragment) fragmentManager.getFragmentFactory()
                                                                          .instantiate(requireContext().getClassLoader(), DirectionFragment.class.getName());
    fragment.setMapObject(mMapObject);
    fragment.show(fragmentManager, null);
  }

  @Override
  public boolean onLongClick(View v)
  {
    final List<String> items = new ArrayList<>();
    final int id = v.getId();
    if (id == R.id.tv__title)
      items.add(mTvTitle.getText().toString());
    else if (id == R.id.tv__secondary_title)
      items.add(mTvSecondaryTitle.getText().toString());
    else if (id == R.id.tv__address)
      items.add(mTvAddress.getText().toString());
    else if (id == R.id.ll__place_latlon)
    {
      final double lat = mMapObject.getLat();
      final double lon = mMapObject.getLon();
      for (CoordinatesFormat format : visibleCoordsFormat)
      {
        String formatted = Framework.nativeFormatLatLon(lat, lon, format.getId());
        if (formatted != null)
          items.add(formatted);
      }
    }
    else if (id == R.id.ll__place_operator)
      items.add(mTvOperator.getText().toString());
    else if (id == R.id.ll__place_level)
      items.add(mTvLevel.getText().toString());
    else if (id == R.id.ll__place_atm)
      items.add(mTvAtm.getText().toString());
    else if (id == R.id.ll__place_capacity)
      items.add(mTvCapacity.getText().toString());
    else if (id == R.id.ll__place_wheelchair)
      items.add(mTvWheelchair.getText().toString());
    else if (id == R.id.ll__place_drive_through)
      items.add(mTvDriveThrough.getText().toString());

    final Context context = requireContext();
    if (items.size() == 1)
      PlacePageUtils.copyToClipboard(context, mFrame, items.get(0));
    else
      PlacePageUtils.showCopyPopup(context, v, items);

    return true;
  }

  private void updateDownloader(CountryItem country)
  {
    if (isInvalidDownloaderStatus(country.status))
    {
      if (mStorageCallbackSlot != 0)
        UiThread.runLater(this::detachCountry);
      return;
    }

    mDownloaderIcon.update(country);

    StringBuilder sb = new StringBuilder(StringUtils.getFileSizeString(requireContext(), country.totalSize));
    if (country.isExpandable())
      sb.append(StringUtils.formatUsingUsLocale("  •  %s: %d", requireContext().getString(R.string.downloader_status_maps),
                                                country.totalChildCount));

    mDownloaderInfo.setText(sb.toString());
  }

  private void updateDownloader()
  {
    if (mCurrentCountry == null)
      return;

    mCurrentCountry.update();
    updateDownloader(mCurrentCountry);
  }

  private void attachCountry(String country)
  {
    CountryItem map = CountryItem.fill(country);
    if (isInvalidDownloaderStatus(map.status))
      return;

    mCurrentCountry = map;
    if (mStorageCallbackSlot == 0)
      mStorageCallbackSlot = MapManager.nativeSubscribe(mStorageCallback);

    mDownloaderIcon.setOnIconClickListener((v) -> MapManager.warn3gAndDownload(requireActivity(), mCurrentCountry.id, null))
                   .setOnCancelClickListener((v) -> MapManager.nativeCancel(mCurrentCountry.id));
    mDownloaderIcon.show(true);
    UiUtils.show(mDownloaderInfo);
    updateDownloader(mCurrentCountry);
  }

  private void detachCountry()
  {
    if (mStorageCallbackSlot == 0 || mCurrentCountry == null)
      return;

    MapManager.nativeUnsubscribe(mStorageCallbackSlot);
    mStorageCallbackSlot = 0;
    mCurrentCountry = null;
    mDownloaderIcon.setOnIconClickListener(null)
                   .setOnCancelClickListener(null);
    mDownloaderIcon.show(false);
    UiUtils.hide(mDownloaderInfo);
  }

  @Override
  public void onChanged(@Nullable MapObject mapObject)
  {
    if (mapObject == null)
      return;
    // Starting the download will fire this callback but the object will be the same
    // Detaching the country in that case will crash the app
    if (!mapObject.sameAs(mMapObject))
      detachCountry();
    setCurrentCountry();

    mMapObject = mapObject;

    refreshViews();
    // In case the place page has already some data, make sure to call the onPlacePageContentChanged callback
    // to catch cases where the new data has the exact same height as the previous one (eg 2 address nodes)
    if (mFrame.getHeight() > 0)
      mPlacePageViewListener.onPlacePageContentChanged(mPreview.getHeight(), mFrame.getHeight());
  }

  @Override
  public void onLocationUpdated(@NonNull Location location)
  {
    if (mMapObject == null)
      return;
    if (mMapObject.isMyPosition())
      refreshMyPosition(location);
    else
      refreshDistanceToObject(location);
  }

  @Override
  public void onCompassUpdated(double north)
  {
    if (mMapObject == null || mMapObject.isMyPosition())
      return;

    final Location location = LocationHelper.from(requireContext()).getSavedLocation();
    if (location == null)
    {
      UiUtils.hide(mAvDirection);
      return;
    }

    final double azimuth = Framework.nativeGetDistanceAndAzimuthFromLatLon(mMapObject.getLat(),
                                                                           mMapObject.getLon(),
                                                                           location.getLatitude(),
                                                                           location.getLongitude(),
                                                                           north)
                                    .getAzimuth();
    UiUtils.showIf(azimuth >= 0, mAvDirection);
    if (azimuth >= 0)
    {
      mAvDirection.setAzimuth(azimuth);
    }
  }

  public interface PlacePageViewListener
  {
    // Called when the content has actually changed and we are ready to compute the peek height
    void onPlacePageContentChanged(int previewHeight, int frameHeight);

    void onPlacePageRequestToggleState();
    void onPlacePageRequestClose();
  }
}
