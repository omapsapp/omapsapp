package com.mapswithme.maps.widget.placepage;

import android.content.Context;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.Color;
import android.location.Location;
import android.text.Html;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.TextUtils;
import android.text.style.ForegroundColorSpan;
import android.text.util.Linkify;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.PopupMenu;
import android.widget.RelativeLayout;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.widget.Toolbar;
import androidx.core.widget.NestedScrollViewClickFixed;
import androidx.fragment.app.Fragment;

import androidx.recyclerview.widget.RecyclerView;
import com.mapswithme.maps.Framework;
import com.mapswithme.maps.MwmActivity;
import com.mapswithme.maps.MwmApplication;
import com.mapswithme.maps.R;
import com.mapswithme.maps.api.ParsedMwmRequest;
import com.mapswithme.maps.bookmarks.data.Bookmark;
import com.mapswithme.maps.bookmarks.data.BookmarkManager;
import com.mapswithme.maps.bookmarks.data.DistanceAndAzimut;
import com.mapswithme.maps.bookmarks.data.MapObject;
import com.mapswithme.maps.bookmarks.data.Metadata;
import com.mapswithme.maps.bookmarks.data.RoadWarningMarkType;
import com.mapswithme.maps.downloader.CountryItem;
import com.mapswithme.maps.downloader.DownloaderStatusIcon;
import com.mapswithme.maps.downloader.MapManager;
import com.mapswithme.maps.editor.Editor;
import com.mapswithme.maps.editor.OpeningHours;
import com.mapswithme.maps.editor.data.TimeFormatUtils;
import com.mapswithme.maps.editor.data.Timetable;
import com.mapswithme.maps.location.LocationHelper;
import com.mapswithme.maps.routing.RoutingController;
import com.mapswithme.maps.search.Popularity;
import com.mapswithme.maps.settings.RoadType;
import com.mapswithme.maps.widget.ArrowView;
import com.mapswithme.util.Graphics;
import com.mapswithme.util.SharingUtils;
import com.mapswithme.util.StringUtils;
import com.mapswithme.util.ThemeUtils;
import com.mapswithme.util.UiUtils;
import com.mapswithme.util.Utils;
import com.mapswithme.util.concurrency.UiThread;
import com.mapswithme.util.log.Logger;
import com.mapswithme.util.log.LoggerFactory;

import java.util.ArrayList;
import java.util.Calendar;
import java.util.List;
import java.util.Locale;
import java.util.Objects;

public class PlacePageView extends NestedScrollViewClickFixed
    implements View.OnClickListener,
               View.OnLongClickListener,
               EditBookmarkFragment.EditBookmarkListener

{
  private static final Logger LOGGER = LoggerFactory.INSTANCE.getLogger(LoggerFactory.Type.MISC);
  private static final String TAG = PlacePageView.class.getSimpleName();
  private static final String PREF_USE_DMS = "use_dms";

  private boolean mIsDocked;
  private boolean mIsFloating;

  // Preview.
  private ViewGroup mPreview;
  private Toolbar mToolbar;
  private TextView mTvTitle;
  private TextView mTvSecondaryTitle;
  private TextView mTvSubtitle;
  private ArrowView mAvDirection;
  private TextView mTvDistance;
  private TextView mTvAddress;
  private RecyclerView mPhoneRecycler;
  private PlacePhoneAdapter mPhoneAdapter;
  private View mWebsite;
  private TextView mTvWebsite;
  private TextView mTvLatlon;
  private SocialLinksView mSocialLinks;
  private View mOpeningHours;
  private TextView mFullOpeningHours;
  private TextView mTodayOpeningHours;
  private View mWifi;
  private View mEmail;
  private TextView mTvEmail;
  private View mOperator;
  private TextView mTvOperator;
  private View mCuisine;
  private TextView mTvCuisine;
  private View mWiki;
  private View mEntrance;
  private TextView mTvEntrance;
  private View mEditPlace;
  private View mAddOrganisation;
  private View mAddPlace;
  private View mEditTopSpace;
  // Bookmark
  private View mBookmarkFrame;
  private WebView mWvBookmarkNote;
  private TextView mTvBookmarkNote;
  private boolean mBookmarkSet;
  // Place page buttons
  private PlacePageButtons mButtons;
  private ImageView mBookmarkButtonIcon;
  @Nullable
  private View mBookmarkButtonFrame;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private View mPlaceDescriptionContainer;

  @SuppressWarnings("NotNullFieldNotInitialized")
  @NonNull
  private View mPlaceDescriptionHeaderContainer;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private TextView mPlaceDescriptionView;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private View mPlaceDescriptionMoreBtn;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private View mPopularityView;

  // Data
  @Nullable
  private MapObject mMapObject;
  private boolean mIsLatLonDms;

  // Downloader`s stuff
  private DownloaderStatusIcon mDownloaderIcon;
  private TextView mDownloaderInfo;
  private int mStorageCallbackSlot;
  @Nullable
  private CountryItem mCurrentCountry;
  private boolean mScrollable = true;

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

  private final Runnable mDownloaderDeferredDetachProc = new Runnable()
  {
    @Override
    public void run()
    {
      detachCountry();
    }
  };
  @NonNull
  private final EditBookmarkClickListener mEditBookmarkClickListener = new EditBookmarkClickListener();

  @NonNull
  private final OnClickListener mDownloadClickListener = new OnClickListener()
  {
    @Override
    public void onClick(View v)
    {
      MapManager.warn3gAndDownload(getActivity(), mCurrentCountry.id, null);
    }
  };

  @NonNull
  private final OnClickListener mCancelDownloadListener = new OnClickListener()
  {
    @Override
    public void onClick(View v)
    {
      MapManager.nativeCancel(mCurrentCountry.id);
    }
  };

  @Nullable
  private Closable mClosable;

  @Nullable
  private RoutingModeListener mRoutingModeListener;

  void setScrollable(boolean scrollable)
  {
    mScrollable = scrollable;
  }

  void addClosable(@NonNull Closable closable)
  {
    mClosable = closable;
  }

  @Override
  public boolean onTouchEvent(MotionEvent ev)
  {
    switch (ev.getAction())
    {
      case MotionEvent.ACTION_DOWN:
        return mScrollable && super.onTouchEvent(ev);
      default:
        return super.onTouchEvent(ev);
    }
  }

  @Override
  public boolean onInterceptTouchEvent(MotionEvent event)
  {
    return mScrollable && super.onInterceptTouchEvent(event);
  }

  public interface SetMapObjectListener
  {
    void onSetMapObjectComplete(boolean isSameObject);
  }

  public PlacePageView(Context context)
  {
    this(context, null, 0);
  }

  public PlacePageView(Context context, AttributeSet attrs)
  {
    this(context, attrs, 0);
  }

  public PlacePageView(Context context, AttributeSet attrs, int defStyleAttr)
  {
    super(context, attrs);
    mIsLatLonDms = MwmApplication.prefs(context).getBoolean(PREF_USE_DMS, false);
    init(attrs, defStyleAttr);
  }

  @Override
  protected void onFinishInflate()
  {
    super.onFinishInflate();
    mPreview = findViewById(R.id.pp__preview);
    mTvTitle = mPreview.findViewById(R.id.tv__title);
    mTvSecondaryTitle = mPreview.findViewById(R.id.tv__secondary_title);
    mToolbar = findViewById(R.id.toolbar);
    mTvSubtitle = mPreview.findViewById(R.id.tv__subtitle);

    View directionFrame = mPreview.findViewById(R.id.direction_frame);
    mTvDistance = mPreview.findViewById(R.id.tv__straight_distance);
    mAvDirection = mPreview.findViewById(R.id.av__direction);
    directionFrame.setOnClickListener(this);

    mTvAddress = mPreview.findViewById(R.id.tv__address);

    RelativeLayout address = findViewById(R.id.ll__place_name);
    mPhoneRecycler = findViewById(R.id.rw__phone);
    mPhoneAdapter = new PlacePhoneAdapter();
    mPhoneRecycler.setAdapter(mPhoneAdapter);
    mWebsite = findViewById(R.id.ll__place_website);
    mWebsite.setOnClickListener(this);
    mTvWebsite = findViewById(R.id.tv__place_website);
    mSocialLinks = findViewById(R.id.place_page_social_links);
    mSocialLinks.setPlacePageView(this);
    LinearLayout latlon = findViewById(R.id.ll__place_latlon);
    latlon.setOnClickListener(this);
    mTvLatlon = findViewById(R.id.tv__place_latlon);
    mOpeningHours = findViewById(R.id.ll__place_schedule);
    mFullOpeningHours = findViewById(R.id.opening_hours);
    mTodayOpeningHours = findViewById(R.id.today_opening_hours);
    mWifi = findViewById(R.id.ll__place_wifi);
    mEmail = findViewById(R.id.ll__place_email);
    mEmail.setOnClickListener(this);
    mTvEmail = findViewById(R.id.tv__place_email);
    mOperator = findViewById(R.id.ll__place_operator);
    mOperator.setOnClickListener(this);
    mTvOperator = findViewById(R.id.tv__place_operator);
    mCuisine = findViewById(R.id.ll__place_cuisine);
    mTvCuisine = findViewById(R.id.tv__place_cuisine);
    mWiki = findViewById(R.id.ll__place_wiki);
    mWiki.setOnClickListener(this);
    mEntrance = findViewById(R.id.ll__place_entrance);
    mTvEntrance = mEntrance.findViewById(R.id.tv__place_entrance);
    mEditPlace = findViewById(R.id.ll__place_editor);
    mEditPlace.setOnClickListener(this);
    mAddOrganisation = findViewById(R.id.ll__add_organisation);
    mAddOrganisation.setOnClickListener(this);
    mAddPlace = findViewById(R.id.ll__place_add);
    mAddPlace.setOnClickListener(this);
    mEditTopSpace = findViewById(R.id.edit_top_space);
    latlon.setOnLongClickListener(this);
    address.setOnLongClickListener(this);
    mWebsite.setOnLongClickListener(this);
    mOpeningHours.setOnLongClickListener(this);
    mEmail.setOnLongClickListener(this);
    mOperator.setOnLongClickListener(this);
    mWiki.setOnLongClickListener(this);

    mBookmarkFrame = findViewById(R.id.bookmark_frame);
    mWvBookmarkNote = mBookmarkFrame.findViewById(R.id.wv__bookmark_notes);
    final WebSettings settings = mWvBookmarkNote.getSettings();
    settings.setJavaScriptEnabled(false);
    settings.setDefaultTextEncodingName("utf-8");
    mTvBookmarkNote = mBookmarkFrame.findViewById(R.id.tv__bookmark_notes);
    initEditMapObjectBtn();

    mDownloaderIcon = new DownloaderStatusIcon(mPreview.findViewById(R.id.downloader_status_frame));

    mDownloaderInfo = mPreview.findViewById(R.id.tv__downloader_details);

    setElevation(UiUtils.dimen(getContext(), R.dimen.placepage_elevation));

    if (UiUtils.isLandscape(getContext()))
      setBackgroundResource(0);

    initPlaceDescriptionView();
  }

  public void initButtons(@NonNull ViewGroup buttons)
  {
    mButtons = new PlacePageButtons(this, buttons, new PlacePageButtons.ItemListener()
    {
      public void onPrepareVisibleView(@NonNull PlacePageButtons.PlacePageButton item,
                                       @NonNull View frame, @NonNull ImageView icon,
                                       @NonNull TextView title)
      {
        int color;

        switch (item.getType())
        {
          case BOOKING:
//          case BOOKING_SEARCH:
          case OPENTABLE:
            // ---------------------------------------------------------------------------------------
            // Warning: the following code is autogenerated.
            // Do NOT change it manually.
            // %PartnersExtender.Switch
          case PARTNER1:
          case PARTNER3:
          case PARTNER18:
          case PARTNER19:
          case PARTNER20:
            // /%PartnersExtender.Switch
            // End of autogenerated code.
            // ---------------------------------------------------------------------------------------
            frame.setBackgroundResource(item.getBackgroundResource());
            color = Color.WHITE;
            break;

          case PARTNER2:
            frame.setBackgroundResource(item.getBackgroundResource());
            color = Color.BLACK;
            break;

          case BOOKMARK:
            mBookmarkButtonIcon = icon;
            mBookmarkButtonFrame = frame;
            updateBookmarkButton();
            color = ThemeUtils.getColor(getContext(), R.attr.iconTint);
            break;

          default:
            color = ThemeUtils.getColor(getContext(), R.attr.iconTint);
            icon.setColorFilter(color);
            break;
        }

        title.setTextColor(color);
      }

      @Override
      public void onItemClick(PlacePageButtons.PlacePageButton item)
      {
        switch (item.getType())
        {
          case BOOKMARK:
            onBookmarkBtnClicked();
            break;

          case SHARE:
            onShareBtnClicked();
            break;

          case BACK:
            onBackBtnClicked();
            break;

          case ROUTE_FROM:
            onRouteFromBtnClicked();
            break;

          case ROUTE_TO:
            onRouteToBtnClicked();
            break;

          case ROUTE_ADD:
            onRouteAddBtnClicked();
            break;

          case ROUTE_REMOVE:
            onRouteRemoveBtnClicked();
            break;

          case ROUTE_AVOID_TOLL:
            onAvoidTollBtnClicked();
            break;

          case ROUTE_AVOID_UNPAVED:
            onAvoidUnpavedBtnClicked();
            break;

          case ROUTE_AVOID_FERRY:
            onAvoidFerryBtnClicked();
            break;

          case CALL:
            onCallBtnClicked(buttons);
            break;
        }
      }
    });
  }

  private void onBookmarkBtnClicked()
  {
    if (mMapObject == null)
    {
      LOGGER.e(TAG, "Bookmark cannot be managed, mMapObject is null!");
      return;
    }
    toggleIsBookmark(mMapObject);
  }

  private void onShareBtnClicked()
  {
    if (mMapObject == null)
    {
      LOGGER.e(TAG, "A map object cannot be shared, it's null!");
      return;
    }
    SharingUtils.shareMapObject(getContext(), mMapObject);
  }

  private void onBackBtnClicked()
  {
    if (mMapObject == null)
    {
      LOGGER.e(TAG, "A mwm request cannot be handled, mMapObject is null!");
      getActivity().finish();
      return;
    }

    if (ParsedMwmRequest.hasRequest())
    {
      ParsedMwmRequest request = ParsedMwmRequest.getCurrentRequest();
      if (ParsedMwmRequest.isPickPointMode())
        request.setPointData(mMapObject.getLat(), mMapObject.getLon(), mMapObject.getTitle(), "");

      request.sendResponseAndFinish(getActivity(), true);
    }
    else
      getActivity().finish();
  }

  private void onRouteFromBtnClicked()
  {
    RoutingController controller = RoutingController.get();
    if (!controller.isPlanning())
    {
      controller.prepare(mMapObject, null);
      close();
    }
    else if (controller.setStartPoint(mMapObject))
    {
      close();
    }
  }

  private void onRouteToBtnClicked()
  {
    if (RoutingController.get().isPlanning())
    {
      RoutingController.get().setEndPoint(mMapObject);
      close();
    }
    else
    {
      getActivity().startLocationToPoint(getMapObject(), true);
    }
  }

  private void onRouteAddBtnClicked()
  {
    if (mMapObject != null)
      RoutingController.get().addStop(mMapObject);
  }

  private void onRouteRemoveBtnClicked()
  {
    if (mMapObject != null)
      RoutingController.get().removeStop(mMapObject);
  }

  private List<String> getAllPhones()
  {
    return mPhoneAdapter.getPhonesList();
  }

  private void onCallBtnClicked(View parentView)
  {
    final List<String> phones = getAllPhones();
    if (phones.size() == 1)
    {
      Utils.callPhone(getContext(), phones.get(0));
    }
    else
    {
      // Show popup menu with all phones
      final PopupMenu popup = new PopupMenu(getContext(), parentView);
      final Menu menu = popup.getMenu();

      for (int i = 0; i < phones.size(); i++)
        menu.add(Menu.NONE, i, i, phones.get(i));

      popup.setOnMenuItemClickListener(item -> {
        final int id = item.getItemId();
        final Context ctx = getContext();
        Utils.callPhone(ctx, phones.get(id));
        return true;
      });

      popup.show();
    }
  }

  public void setRoutingModeListener(@Nullable RoutingModeListener routingModeListener)
  {
    mRoutingModeListener = routingModeListener;
  }

  private void onAvoidUnpavedBtnClicked()
  {
    onAvoidBtnClicked(RoadType.Dirty);
  }

  private void onAvoidFerryBtnClicked()
  {
    onAvoidBtnClicked(RoadType.Ferry);
  }

  private void onAvoidTollBtnClicked()
  {
    onAvoidBtnClicked(RoadType.Toll);
  }

  private void onAvoidBtnClicked(@NonNull RoadType roadType)
  {
    if (mRoutingModeListener == null)
      return;

    mRoutingModeListener.toggleRouteSettings(roadType);
  }

  private void initPlaceDescriptionView()
  {
    mPlaceDescriptionContainer = findViewById(R.id.poi_description_container);
    mPlaceDescriptionView = findViewById(R.id.poi_description);
    mPlaceDescriptionHeaderContainer = findViewById(R.id.pp_description_header_container);
    mPlaceDescriptionMoreBtn = findViewById(R.id.more_btn);
    mPlaceDescriptionMoreBtn.setOnClickListener(v -> showDescriptionScreen());
  }

  private void showDescriptionScreen()
  {
    Context context = mPlaceDescriptionContainer.getContext();
    String description = Objects.requireNonNull(mMapObject).getDescription();
    PlaceDescriptionActivity.start(context, description);
  }

  private void initEditMapObjectBtn()
  {
    final View editBookmarkBtn = mBookmarkFrame.findViewById(R.id.tv__bookmark_edit);
    editBookmarkBtn.setVisibility(View.VISIBLE);
    editBookmarkBtn.setOnClickListener(mEditBookmarkClickListener);
  }

  private void init(AttributeSet attrs, int defStyleAttr)
  {
    LayoutInflater.from(getContext()).inflate(R.layout.place_page, this);

    if (isInEditMode())
      return;

    final TypedArray attrArray = getContext().obtainStyledAttributes(attrs, R.styleable.PlacePageView, defStyleAttr, 0);
    mIsDocked = attrArray.getBoolean(R.styleable.PlacePageView_docked, false);
    mIsFloating = attrArray.getBoolean(R.styleable.PlacePageView_floating, false);
    attrArray.recycle();
  }

  public boolean isDocked()
  {
    return mIsDocked;
  }

  public boolean isFloating()
  {
    return mIsFloating;
  }

  @Nullable
  public MapObject getMapObject()
  {
    return mMapObject;
  }

  /**
   * @param mapObject new MapObject
   * @param listener  listener
   */
  public void setMapObject(@Nullable MapObject mapObject, @Nullable final SetMapObjectListener listener)
  {
    if (MapObject.same(mMapObject, mapObject))
    {
      mMapObject = mapObject;
      refreshViews();
      if (listener != null)
      {
        listener.onSetMapObjectComplete(true);
      }
      if (mCurrentCountry == null)
        setCurrentCountry();
      return;
    }

    mMapObject = mapObject;

    setMapObjectInternal();
    if (listener != null)
      listener.onSetMapObjectComplete(false);
  }

  private void setMapObjectInternal()
  {
    detachCountry();
    if (mMapObject != null)
    {
      initEditMapObjectBtn();

      setCurrentCountry();
    }
    refreshViews();
  }

  private void setCurrentCountry()
  {
    if (mCurrentCountry != null)
      throw new AssertionError("country should be detached before!");
    String country = MapManager.nativeGetSelectedCountry();
    if (country != null && !RoutingController.get().isNavigating())
      attachCountry(country);
  }

  void refreshViews()
  {
    if (mMapObject == null)
    {
      LOGGER.e(TAG, "A place page views cannot be refreshed, mMapObject is null");
      return;
    }
    refreshPreview(mMapObject);
    refreshDetails(mMapObject);
    refreshViewsInternal(mMapObject);
  }

  private void refreshViewsInternal(@NonNull MapObject mapObject)
  {
    final Location loc = LocationHelper.INSTANCE.getSavedLocation();
    switch (mapObject.getMapObjectType())
    {
      case MapObject.BOOKMARK:
        refreshDistanceToObject(mapObject, loc);
        showBookmarkDetails(mapObject);
        updateBookmarkButton();
        setButtons(mapObject, false, true);
        break;
      case MapObject.POI:
      case MapObject.SEARCH:
        refreshDistanceToObject(mapObject, loc);
        hideBookmarkDetails();
        setButtons(mapObject, false, true);
        setPlaceDescription(mapObject);
        break;
      case MapObject.API_POINT:
        refreshDistanceToObject(mapObject, loc);
        hideBookmarkDetails();
        setButtons(mapObject, true, true);
        break;
      case MapObject.MY_POSITION:
        refreshMyPosition(mapObject, loc);
        hideBookmarkDetails();
        setButtons(mapObject, false, false);
        break;
    }
  }

  private void setPlaceDescription(@NonNull MapObject mapObject)
  {
    boolean isBookmark = MapObject.isOfType(MapObject.BOOKMARK, mapObject);
    if (TextUtils.isEmpty(mapObject.getDescription()) && !isBookmark)
    {
      UiUtils.hide(mPlaceDescriptionContainer, mPlaceDescriptionHeaderContainer);
      return;
    }

    if (isBookmark)
    {
      final Bookmark bmk = (Bookmark) mapObject;
      UiUtils.showIf(!TextUtils.isEmpty(bmk.getBookmarkDescription()), mPlaceDescriptionHeaderContainer);
      UiUtils.hide(mPlaceDescriptionContainer);
      return;
    }
    UiUtils.show(mPlaceDescriptionContainer, mPlaceDescriptionHeaderContainer);
    mPlaceDescriptionView.setText(Html.fromHtml(mapObject.getDescription()));
  }

  private void setTextAndColorizeSubtitle(@NonNull MapObject mapObject)
  {
    String text = mapObject.getSubtitle();
    UiUtils.setTextAndHideIfEmpty(mTvSubtitle, text);
    if (!TextUtils.isEmpty(text))
    {
      SpannableStringBuilder sb = new SpannableStringBuilder(text);
      int start = text.indexOf("★");
      if (start > -1)
      {
        sb.setSpan(new ForegroundColorSpan(getResources().getColor(R.color.base_yellow)),
                   start, sb.length(), Spanned.SPAN_INCLUSIVE_EXCLUSIVE);

      }
      mTvSubtitle.setText(sb);
    }
  }

  private void refreshPreview(@NonNull MapObject mapObject)
  {
    UiUtils.setTextAndHideIfEmpty(mTvTitle, mapObject.getTitle());
    UiUtils.setTextAndHideIfEmpty(mTvSecondaryTitle, mapObject.getSecondaryTitle());
    boolean isPopular = mapObject.getPopularity().getType() == Popularity.Type.POPULAR;
    if (mPopularityView != null)
      UiUtils.showIf(isPopular, mPopularityView);
    if (mToolbar != null)
      mToolbar.setTitle(mapObject.getTitle());
    setTextAndColorizeSubtitle(mapObject);
    UiUtils.hide(mAvDirection);
    UiUtils.setTextAndHideIfEmpty(mTvAddress, mapObject.getAddress());
  }

  private void refreshDetails(@NonNull MapObject mapObject)
  {
    refreshLatLon(mapObject);

    String website = mapObject.getMetadata(Metadata.MetadataType.FMD_WEBSITE);
    String url = mapObject.getMetadata(Metadata.MetadataType.FMD_URL);
    refreshMetadataOrHide(TextUtils.isEmpty(website) ? url : website, mWebsite, mTvWebsite);
    refreshPhoneNumberList(mapObject.getMetadata(Metadata.MetadataType.FMD_PHONE_NUMBER));
    refreshMetadataOrHide(mapObject.getMetadata(Metadata.MetadataType.FMD_EMAIL), mEmail, mTvEmail);
    refreshMetadataOrHide(mapObject.getMetadata(Metadata.MetadataType.FMD_OPERATOR), mOperator, mTvOperator);
    refreshMetadataOrHide(Framework.nativeGetActiveObjectFormattedCuisine(), mCuisine, mTvCuisine);
    refreshMetadataOrHide(mapObject.getMetadata(Metadata.MetadataType.FMD_WIKIPEDIA), mWiki, null);
    refreshMetadataOrHide(mapObject.getMetadata(Metadata.MetadataType.FMD_INTERNET), mWifi, null);
    refreshMetadataOrHide(mapObject.getMetadata(Metadata.MetadataType.FMD_FLATS), mEntrance, mTvEntrance);
    refreshOpeningHours(mapObject);
    refreshSocialLinks(mapObject);

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
    setPlaceDescription(mapObject);
  }

  private void refreshOpeningHours(@NonNull MapObject mapObject)
  {
    final String ohStr = mapObject.getMetadata(Metadata.MetadataType.FMD_OPEN_HOURS);
    final Timetable[] timetables = OpeningHours.nativeTimetablesFromString(ohStr);
    final boolean isEmptyTT = (timetables == null || timetables.length == 0);
    final int color = ThemeUtils.getColor(getContext(), android.R.attr.textColorPrimary);

    String ohStringToShow = null;

    if (isEmptyTT)
    {
      if (!ohStr.isEmpty())
        ohStringToShow = ohStr;
      else
      {
        UiUtils.hide(mOpeningHours);
        return;
      }
    }

    UiUtils.show(mOpeningHours);

    final Resources resources = getResources();
    if (!isEmptyTT && timetables[0].isFullWeek())
    {
      ohStringToShow = timetables[0].isFullday ? resources.getString(R.string.twentyfour_seven)
                                               : resources.getString(R.string.daily) + " " + timetables[0].workingTimespan;
    }

    if (ohStringToShow != null)
    {
      refreshTodayOpeningHours(ohStringToShow, color);
      UiUtils.clearTextAndHide(mFullOpeningHours);
      return;
    }

    boolean containsCurrentWeekday = false;
    final int currentDay = Calendar.getInstance().get(Calendar.DAY_OF_WEEK);
    for (Timetable tt : timetables)
    {
      if (tt.containsWeekday(currentDay))
      {
        containsCurrentWeekday = true;
        String workingTime;

        if (tt.isFullday)
        {
          String allDay = resources.getString(R.string.editor_time_allday);
          workingTime = Utils.unCapitalize(allDay);
        }
        else
        {
          workingTime = tt.workingTimespan.toString();
        }

        refreshTodayOpeningHours(resources.getString(R.string.today) + " " + workingTime, color);
        break;
      }
    }

    UiUtils.setTextAndShow(mFullOpeningHours, TimeFormatUtils.formatTimetables(getContext(), timetables));
    if (!containsCurrentWeekday)
      refreshTodayOpeningHours(resources.getString(R.string.day_off_today), resources.getColor(R.color.base_red));
  }

  private void refreshSocialLinks(@NonNull MapObject mapObject)
  {
    final String facebookPageLink = mapObject.getMetadata(Metadata.MetadataType.FMD_FACEBOOK_PAGE);
    final String instagramPageLink = mapObject.getMetadata(Metadata.MetadataType.FMD_INSTAGRAM_PAGE);
    final String twitterPageLink = mapObject.getMetadata(Metadata.MetadataType.FMD_TWITTER_PAGE);
    final String vkPageLink = mapObject.getMetadata(Metadata.MetadataType.FMD_VK_PAGE);

    mSocialLinks.refreshSocialLinks(facebookPageLink, instagramPageLink, twitterPageLink, vkPageLink);
  }

  private void refreshTodayOpeningHours(String text, @ColorInt int color)
  {
    UiUtils.setTextAndShow(mTodayOpeningHours, text);
    mTodayOpeningHours.setTextColor(color);
  }

  private void refreshPhoneNumberList(String phones)
  {
    mPhoneAdapter.refreshPhones(phones);
  }

  private void updateBookmarkButton()
  {
    if (mBookmarkButtonIcon == null || mBookmarkButtonFrame == null)
      return;

    if (mBookmarkSet)
      mBookmarkButtonIcon.setImageDrawable(Graphics.tint(getContext(), R.drawable.ic_bookmarks_on, R.attr.iconTintActive));
    else
      mBookmarkButtonIcon.setImageDrawable(Graphics.tint(getContext(), R.drawable.ic_bookmarks_off, R.attr.iconTint));

    mBookmarkButtonFrame.setEnabled(true);
  }

  private void hideBookmarkDetails()
  {
    mBookmarkSet = false;
    UiUtils.hide(mBookmarkFrame);
    updateBookmarkButton();
  }

  private void showBookmarkDetails(@NonNull MapObject mapObject)
  {
    mBookmarkSet = true;
    UiUtils.show(mBookmarkFrame);

    final String notes = ((Bookmark) mapObject).getBookmarkDescription();

    if (TextUtils.isEmpty(notes))
    {
      UiUtils.hide(mTvBookmarkNote, mWvBookmarkNote);
      return;
    }

    if (StringUtils.nativeIsHtml(notes))
    {
      mWvBookmarkNote.loadData(notes, Utils.TEXT_HTML, Utils.UTF_8);
      UiUtils.show(mWvBookmarkNote);
      UiUtils.hide(mTvBookmarkNote);
    }
    else
    {
      mTvBookmarkNote.setText(notes);
      Linkify.addLinks(mTvBookmarkNote, Linkify.ALL);
      UiUtils.show(mTvBookmarkNote);
      UiUtils.hide(mWvBookmarkNote);
    }
  }

  @NonNull
  private static PlacePageButtons.Item toPlacePageButton(@NonNull RoadWarningMarkType type)
  {
    switch (type)
    {
      case DIRTY:
        return PlacePageButtons.Item.ROUTE_AVOID_UNPAVED;
      case FERRY:
        return PlacePageButtons.Item.ROUTE_AVOID_FERRY;
      case TOLL:
        return PlacePageButtons.Item.ROUTE_AVOID_TOLL;
      default:
        throw new AssertionError("Unsupported road warning type: " + type);
    }
  }

  private void setButtons(@NonNull MapObject mapObject, boolean showBackButton, boolean showRoutingButton)
  {
    List<PlacePageButtons.PlacePageButton> buttons = new ArrayList<>();

    if (mapObject.getRoadWarningMarkType() != RoadWarningMarkType.UNKNOWN)
    {
      RoadWarningMarkType markType = mapObject.getRoadWarningMarkType();
      PlacePageButtons.Item roadType = toPlacePageButton(markType);
      buttons.add(roadType);
      mButtons.setItems(buttons);
      return;
    }

    if (RoutingController.get().isRoutePoint(mapObject))
    {
      buttons.add(PlacePageButtons.Item.ROUTE_REMOVE);
      mButtons.setItems(buttons);
      return;
    }

    if (showBackButton || ParsedMwmRequest.isPickPointMode())
      buttons.add(PlacePageButtons.Item.BACK);

    final boolean hasNumber = mapObject.hasPhoneNumber();

    if (hasNumber)
    {
      buttons.add(PlacePageButtons.Item.CALL);
      mPhoneRecycler.setVisibility(VISIBLE);
    }
    else
      mPhoneRecycler.setVisibility(GONE);

    boolean needToShowRoutingButtons = RoutingController.get().isPlanning() || showRoutingButton;

    if (needToShowRoutingButtons && !hasNumber)
      buttons.add(PlacePageButtons.Item.ROUTE_FROM);

    buttons.add(mapObject.getMapObjectType() == MapObject.BOOKMARK ? PlacePageButtons.Item.BOOKMARK_DELETE
        : PlacePageButtons.Item.BOOKMARK_SAVE);

    if (needToShowRoutingButtons)
    {
      buttons.add(PlacePageButtons.Item.ROUTE_TO);
      if (hasNumber)
        buttons.add(PlacePageButtons.Item.ROUTE_FROM);
      if (RoutingController.get().isStopPointAllowed())
        buttons.add(PlacePageButtons.Item.ROUTE_ADD);
    }

    buttons.add(PlacePageButtons.Item.SHARE);

    mButtons.setItems(buttons);
  }

  public void refreshLocation(Location l)
  {
    if (mMapObject == null)
    {
      // TODO: This method is constantly called even when nothing is selected on the map.
      //LOGGER.e(TAG, "A location cannot be refreshed, mMapObject is null!");
      return;
    }

    if (MapObject.isOfType(MapObject.MY_POSITION, mMapObject))
      refreshMyPosition(mMapObject, l);
    else
      refreshDistanceToObject(mMapObject, l);
  }

  private void refreshMyPosition(@NonNull MapObject mapObject, Location l)
  {
    UiUtils.hide(mTvDistance);

    if (l == null)
      return;

    final StringBuilder builder = new StringBuilder();
    if (l.hasAltitude())
    {
      double altitude = l.getAltitude();
      builder.append(altitude >= 0 ? "▲" : "▼");
      builder.append(Framework.nativeFormatAltitude(altitude));
    }
    if (l.hasSpeed())
      builder.append("   ")
             .append(Framework.nativeFormatSpeed(l.getSpeed()));
    UiUtils.setTextAndHideIfEmpty(mTvSubtitle, builder.toString());

    mapObject.setLat(l.getLatitude());
    mapObject.setLon(l.getLongitude());
    refreshLatLon(mapObject);
  }

  private void refreshDistanceToObject(@NonNull MapObject mapObject, Location l)
  {
    UiUtils.showIf(l != null, mTvDistance);
    if (l == null)
      return;

    mTvDistance.setVisibility(View.VISIBLE);
    double lat = mapObject.getLat();
    double lon = mapObject.getLon();
    DistanceAndAzimut distanceAndAzimuth =
        Framework.nativeGetDistanceAndAzimuthFromLatLon(lat, lon, l.getLatitude(), l.getLongitude(), 0.0);
    mTvDistance.setText(distanceAndAzimuth.getDistance());
  }

  private void refreshLatLon(@NonNull MapObject mapObject)
  {
    final double lat = mapObject.getLat();
    final double lon = mapObject.getLon();
    final String[] latLon = Framework.nativeFormatLatLonToArr(lat, lon, mIsLatLonDms);
    if (latLon.length == 2)
      mTvLatlon.setText(String.format(Locale.US, "%1$s, %2$s", latLon[0], latLon[1]));
  }

  private static void refreshMetadataOrHide(String metadata, View metaLayout, TextView metaTv)
  {
    if (!TextUtils.isEmpty(metadata))
    {
      metaLayout.setVisibility(View.VISIBLE);
      if (metaTv != null)
        metaTv.setText(metadata);
    }
    else
      metaLayout.setVisibility(View.GONE);
  }

  public void refreshAzimuth(double northAzimuth)
  {
    if (mMapObject == null || MapObject.isOfType(MapObject.MY_POSITION, mMapObject))
      return;

    final Location location = LocationHelper.INSTANCE.getSavedLocation();
    if (location == null)
      return;

    final double azimuth = Framework.nativeGetDistanceAndAzimuthFromLatLon(mMapObject.getLat(),
                                                                           mMapObject.getLon(),
                                                                           location.getLatitude(),
                                                                           location.getLongitude(),
                                                                           northAzimuth).getAzimuth();
    if (azimuth >= 0)
    {
      UiUtils.show(mAvDirection);
      mAvDirection.setAzimuth(azimuth);
    }
  }

  private void addOrganisation()
  {
    getActivity().showPositionChooser(true, false);
  }

  private void addPlace()
  {
    getActivity().showPositionChooser(false, true);
  }

  @Override
  public void onClick(View v)
  {
    switch (v.getId())
    {
      case R.id.ll__place_editor:
        if (mMapObject == null)
        {
          LOGGER.e(TAG, "Cannot start editor, map object is null!");
          break;
        }
        getActivity().showEditor();
        break;
      case R.id.ll__add_organisation:
        addOrganisation();
        break;
      case R.id.ll__place_add:
        addPlace();
        break;
      case R.id.ll__place_latlon:
        mIsLatLonDms = !mIsLatLonDms;
        MwmApplication.prefs(getContext()).edit().putBoolean(PREF_USE_DMS, mIsLatLonDms).apply();
        if (mMapObject == null)
        {
          LOGGER.e(TAG, "A LatLon cannot be refreshed, mMapObject is null");
          break;
        }
        refreshLatLon(mMapObject);
        break;
      case R.id.ll__place_website:
        Utils.openUrl(getContext(), mTvWebsite.getText().toString());
        break;
      case R.id.ll__place_wiki:
        // TODO: Refactor and use separate getters for Wiki and all other PP meta info too.
        if (mMapObject == null)
        {
          LOGGER.e(TAG, "Cannot follow url, mMapObject is null!");
          break;
        }
        Utils.openUrl(getContext(), mMapObject.getMetadata(Metadata.MetadataType.FMD_WIKIPEDIA));
        break;
      case R.id.direction_frame:
        showBigDirection();
        break;
      case R.id.ll__place_email:
        Utils.sendTo(getContext(), mTvEmail.getText().toString());
        break;
    }
  }

  private void toggleIsBookmark(@NonNull MapObject mapObject)
  {
    if (MapObject.isOfType(MapObject.BOOKMARK, mapObject))
      setMapObject(Framework.nativeDeleteBookmarkFromMapObject(), null);
    else
      setMapObject(BookmarkManager.INSTANCE.addNewBookmark(mapObject.getLat(), mapObject.getLon()), null);
  }

  private void showBigDirection()
  {
    final DirectionFragment fragment = (DirectionFragment) Fragment.instantiate(getActivity(), DirectionFragment.class
        .getName(), null);
    fragment.setMapObject(mMapObject);
    fragment.show(getActivity().getSupportFragmentManager(), null);
  }

  @Override
  public boolean onLongClick(View v)
  {
    final List<String> items = new ArrayList<>();
    switch (v.getId())
    {
      case R.id.ll__place_latlon:
        if (mMapObject == null)
        {
          LOGGER.e(TAG, "A long click tap on LatLon cannot be handled, mMapObject is null!");
          break;
        }
        final double lat = mMapObject.getLat();
        final double lon = mMapObject.getLon();
        items.add(Framework.nativeFormatLatLon(lat, lon, false));
        items.add(Framework.nativeFormatLatLon(lat, lon, true));
        break;
      case R.id.ll__place_website:
        items.add(mTvWebsite.getText().toString());
        break;
      case R.id.ll__place_email:
        items.add(mTvEmail.getText().toString());
        break;
      case R.id.ll__place_schedule:
        String text = UiUtils.isVisible(mFullOpeningHours)
                      ? mFullOpeningHours.getText().toString()
                      : mTodayOpeningHours.getText().toString();
        items.add(text);
        break;
      case R.id.ll__place_operator:
        items.add(mTvOperator.getText().toString());
        break;
      case R.id.ll__place_wiki:
        if (mMapObject == null)
        {
          LOGGER.e(TAG, "A long click tap on wiki cannot be handled, mMapObject is null!");
          break;
        }
        items.add(mMapObject.getMetadata(Metadata.MetadataType.FMD_WIKIPEDIA));
        break;
    }

    showCopyMenu(v, items);
    return true;
  }

  public void showCopyMenu(View anchorView, List<String> items)
  {
    final PopupMenu popup = new PopupMenu(getContext(), anchorView);
    final Menu menu = popup.getMenu();
    final String copyText = getResources().getString(android.R.string.copy);
    for (int i = 0; i < items.size(); i++)
      menu.add(Menu.NONE, i, i, String.format("%s %s", copyText, items.get(i)));

    popup.setOnMenuItemClickListener(item -> {
      final int id = item.getItemId();
      final Context ctx = getContext();
      Utils.copyTextToClipboard(ctx, items.get(id));
      Utils.showSnackbarAbove(findViewById(R.id.pp__details_frame),
                              getRootView().findViewById(R.id.menu_frame),
                              ctx.getString(R.string.copied_to_clipboard, items.get(id)));
      return true;
    });

    popup.show();
  }

  private void close()
  {
    if (mClosable != null)
      mClosable.closePlacePage();
  }

  void reset()
  {
    resetScroll();
    detachCountry();
  }

  void resetScroll()
  {
    scrollTo(0, 0);
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

  private void updateDownloader(CountryItem country)
  {
    if (isInvalidDownloaderStatus(country.status))
    {
      if (mStorageCallbackSlot != 0)
        UiThread.runLater(mDownloaderDeferredDetachProc);
      return;
    }

    mDownloaderIcon.update(country);

    StringBuilder sb = new StringBuilder(StringUtils.getFileSizeString(getContext(), country.totalSize));
    if (country.isExpandable())
      sb.append(String.format(Locale.US, "  •  %s: %d", getContext().getString(R.string.downloader_status_maps), country.totalChildCount));

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

    mDownloaderIcon.setOnIconClickListener(mDownloadClickListener)
                   .setOnCancelClickListener(mCancelDownloadListener);
    mDownloaderIcon.show(true);
    UiUtils.show(mDownloaderInfo);
    updateDownloader(mCurrentCountry);
  }

  private void detachCountry()
  {
    if (mStorageCallbackSlot == 0)
      return;

    MapManager.nativeUnsubscribe(mStorageCallbackSlot);
    mStorageCallbackSlot = 0;
    mCurrentCountry = null;
    mDownloaderIcon.setOnIconClickListener(null)
                   .setOnCancelClickListener(null);
    mDownloaderIcon.show(false);
    UiUtils.hide(mDownloaderInfo);
  }

  MwmActivity getActivity()
  {
    return (MwmActivity) getContext();
  }

  @Override
  public void onBookmarkSaved(long bookmarkId, boolean movedFromCategory)
  {
    Bookmark updatedBookmark = BookmarkManager.INSTANCE.updateBookmarkPlacePage(bookmarkId);
    if (updatedBookmark == null)
      return;

    setMapObject(updatedBookmark, null);
    refreshViews();
  }

  int getPreviewHeight()
  {
    return mPreview.getHeight();
  }

  private class EditBookmarkClickListener implements OnClickListener
  {
    @Override
    public void onClick(View v)
    {
      if (mMapObject == null)
      {
        LOGGER.e(TAG, "A bookmark cannot be edited, mMapObject is null!");
        return;
      }
      Bookmark bookmark = (Bookmark) mMapObject;
      EditBookmarkFragment.editBookmark(bookmark.getCategoryId(),
                                        bookmark.getBookmarkId(),
                                        getActivity(),
                                        getActivity().getSupportFragmentManager(),
                                        PlacePageView.this);
    }
  }
}
