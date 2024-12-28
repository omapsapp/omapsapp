package app.organicmaps.widget.placepage.sections;

import android.os.Bundle;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.Observer;
import androidx.lifecycle.ViewModelProvider;

import app.organicmaps.Framework;
import app.organicmaps.MwmActivity;
import app.organicmaps.R;
import app.organicmaps.bookmarks.data.MapObject;
import app.organicmaps.bookmarks.data.Metadata;
import app.organicmaps.util.Config;
import app.organicmaps.util.UiUtils;
import app.organicmaps.util.Utils;
import app.organicmaps.widget.placepage.PlacePageUtils;
import app.organicmaps.widget.placepage.PlacePageViewModel;

import java.util.ArrayList;
import java.util.List;

import static android.view.View.GONE;
import static android.view.View.VISIBLE;

public class PlacePageLinksFragment extends Fragment implements Observer<MapObject>
{
  private static final String TAG = PlacePageLinksFragment.class.getSimpleName();

  private View mFrame;
  private View mFacebookPage;
  private TextView mTvFacebookPage;
  private View mInstagramPage;
  private TextView mTvInstagramPage;
  private View mTwitterPage;
  private TextView mTvTwitterPage;
  private View mVkPage;
  private TextView mTvVkPage;
  private View mLinePage;
  private TextView mTvLinePage;

  private View mKayak;
  private View mWebsite;
  private TextView mTvWebsite;
  private View mWebsiteMenu;
  private TextView mTvWebsiteMenuSubsite;
  private View mEmail;
  private TextView mTvEmail;
  private View mWikimedia;
  private TextView mTvWikimedia;

  private PlacePageViewModel mViewModel;
  private MapObject mMapObject;

  private static void refreshMetadataOrHide(@Nullable String metadata, @NonNull View metaLayout,
                                            @NonNull TextView metaTv)
  {
    if (!TextUtils.isEmpty(metadata))
    {
      metaLayout.setVisibility(VISIBLE);
      metaTv.setText(metadata);
    }
    else
      metaLayout.setVisibility(GONE);
  }

  @NonNull
  private String getLink(@NonNull Metadata.MetadataType type)
  {
    return switch (type)
    {
      case FMD_EXTERNAL_URI -> mMapObject.getKayakUrl();
      case FMD_WEBSITE ->
          mMapObject.getWebsiteUrl(false /* strip */, Metadata.MetadataType.FMD_WEBSITE);
      case FMD_WEBSITE_MENU ->
          mMapObject.getWebsiteUrl(false /* strip */, Metadata.MetadataType.FMD_WEBSITE_MENU);
      case FMD_CONTACT_FACEBOOK, FMD_CONTACT_INSTAGRAM, FMD_CONTACT_TWITTER, FMD_CONTACT_VK, FMD_CONTACT_LINE ->
      {
        if (TextUtils.isEmpty(mMapObject.getMetadata(type)))
          yield "";
        yield Framework.nativeGetPoiContactUrl(type.toInt());
      }
      default -> mMapObject.getMetadata(type);
    };
  }

  @Nullable
  @Override
  public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState)
  {
    mViewModel = new ViewModelProvider(requireActivity()).get(PlacePageViewModel.class);
    return inflater.inflate(R.layout.place_page_links_fragment, container, false);
  }

  @Override
  public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);
    mFrame = view;

    mKayak = mFrame.findViewById(R.id.ll__place_kayak);
    mKayak.setOnClickListener((v) -> {
      final String url = mMapObject.getKayakUrl();
      final MwmActivity activity = (MwmActivity) requireActivity();
      if (!TextUtils.isEmpty(url))
        activity.openKayakLink(url);
    });
    mKayak.setOnLongClickListener((v) -> {
      final String url = mMapObject.getKayakUrl();
      if (!TextUtils.isEmpty(url))
        PlacePageUtils.copyToClipboard(requireContext(), mFrame, url);
      return true;
    });

    mWebsite = mFrame.findViewById(R.id.ll__place_website);
    mTvWebsite = mFrame.findViewById(R.id.tv__place_website);
    mWebsite.setOnClickListener((v) -> openUrl(Metadata.MetadataType.FMD_WEBSITE));
    mWebsite.setOnLongClickListener((v) -> copyUrl(mWebsite, Metadata.MetadataType.FMD_WEBSITE));

    mWebsiteMenu = mFrame.findViewById(R.id.ll__place_website_menu);
    mTvWebsiteMenuSubsite = mFrame.findViewById(R.id.tv__place_website_menu_subtitle);
    mWebsiteMenu.setOnClickListener((v) -> openUrl(Metadata.MetadataType.FMD_WEBSITE_MENU));
    mWebsiteMenu.setOnLongClickListener((v) -> copyUrl(mWebsiteMenu, Metadata.MetadataType.FMD_WEBSITE_MENU));

    mEmail = mFrame.findViewById(R.id.ll__place_email);
    mTvEmail = mFrame.findViewById(R.id.tv__place_email);
    mEmail.setOnClickListener(v -> {
      final String email = mMapObject.getMetadata(Metadata.MetadataType.FMD_EMAIL);
      if (!TextUtils.isEmpty(email))
        Utils.sendTo(requireContext(), email);
    });
    mEmail.setOnLongClickListener((v) -> copyUrl(mEmail, Metadata.MetadataType.FMD_EMAIL));

    mWikimedia = mFrame.findViewById(R.id.ll__place_wikimedia);
    mTvWikimedia = mFrame.findViewById(R.id.tv__place_wikimedia);
    mWikimedia.setOnClickListener((v) -> openUrl(Metadata.MetadataType.FMD_WIKIMEDIA_COMMONS));
    mWikimedia.setOnLongClickListener((v) -> copyUrl(mWikimedia, Metadata.MetadataType.FMD_WIKIMEDIA_COMMONS));

    mFacebookPage = mFrame.findViewById(R.id.ll__place_facebook);
    mTvFacebookPage = mFrame.findViewById(R.id.tv__place_facebook_page);
    mFacebookPage.setOnClickListener((v) -> openUrl(Metadata.MetadataType.FMD_CONTACT_FACEBOOK));
    mFacebookPage.setOnLongClickListener((v) -> copyUrl(mFacebookPage, Metadata.MetadataType.FMD_CONTACT_FACEBOOK));

    mInstagramPage = mFrame.findViewById(R.id.ll__place_instagram);
    mTvInstagramPage = mFrame.findViewById(R.id.tv__place_instagram_page);
    mInstagramPage.setOnClickListener((v) -> openUrl(Metadata.MetadataType.FMD_CONTACT_INSTAGRAM));
    mInstagramPage.setOnLongClickListener((v) -> copyUrl(mInstagramPage, Metadata.MetadataType.FMD_CONTACT_INSTAGRAM));

    mTwitterPage = mFrame.findViewById(R.id.ll__place_twitter);
    mTvTwitterPage = mFrame.findViewById(R.id.tv__place_twitter_page);
    mTwitterPage.setOnClickListener((v) -> openUrl(Metadata.MetadataType.FMD_CONTACT_TWITTER));
    mTwitterPage.setOnLongClickListener((v) -> copyUrl(mTwitterPage, Metadata.MetadataType.FMD_CONTACT_TWITTER));

    mVkPage = mFrame.findViewById(R.id.ll__place_vk);
    mTvVkPage = mFrame.findViewById(R.id.tv__place_vk_page);
    mVkPage.setOnClickListener((v) -> openUrl(Metadata.MetadataType.FMD_CONTACT_VK));
    mVkPage.setOnLongClickListener((v) -> copyUrl(mVkPage, Metadata.MetadataType.FMD_CONTACT_VK));

    mLinePage = mFrame.findViewById(R.id.ll__place_line);
    mTvLinePage = mFrame.findViewById(R.id.tv__place_line_page);
    mLinePage.setOnClickListener((v) -> openUrl(Metadata.MetadataType.FMD_CONTACT_LINE));
    mLinePage.setOnLongClickListener((v) -> copyUrl(mLinePage, Metadata.MetadataType.FMD_CONTACT_LINE));
  }

  private void openUrl(Metadata.MetadataType type)
  {
    final String url = getLink(type);
    if (!TextUtils.isEmpty(url))
      Utils.openUrl(requireContext(), url);
  }

  private boolean copyUrl(View view, Metadata.MetadataType type)
  {
    final String url = getLink(type);
    if (TextUtils.isEmpty(url))
      return false;
    final List<String> items = new ArrayList<>();
    items.add(url);

    final String title = switch (type){
      case FMD_WEBSITE -> mMapObject.getWebsiteUrl(false /* strip */, Metadata.MetadataType.FMD_WEBSITE);
      case FMD_WEBSITE_MENU -> mMapObject.getWebsiteUrl(false /* strip */, Metadata.MetadataType.FMD_WEBSITE_MENU);
      default -> mMapObject.getMetadata(type);
    };
    // Add user names for social media if available
    if (!TextUtils.isEmpty(title) && !title.equals(url) && !title.contains("/"))
      items.add(title);

    if (items.size() == 1)
      PlacePageUtils.copyToClipboard(requireContext(), mFrame, items.get(0));
    else
      PlacePageUtils.showCopyPopup(requireContext(), view, items);
    return true;
  }

  private void refreshLinks()
  {
    refreshMetadataOrHide(mMapObject.getWebsiteUrl(true /* strip */, Metadata.MetadataType.FMD_WEBSITE), mWebsite, mTvWebsite);
    refreshMetadataOrHide(mMapObject.getWebsiteUrl(true /* strip */, Metadata.MetadataType.FMD_WEBSITE_MENU), mWebsiteMenu, mTvWebsiteMenuSubsite);

    String wikimedia_commons = mMapObject.getMetadata(Metadata.MetadataType.FMD_WIKIMEDIA_COMMONS);
    String wikimedia_commons_text = TextUtils.isEmpty(wikimedia_commons) ? "" : getResources().getString(R.string.wikimedia_commons);
    refreshMetadataOrHide(wikimedia_commons_text, mWikimedia, mTvWikimedia);
    refreshMetadataOrHide(mMapObject.getMetadata(Metadata.MetadataType.FMD_EMAIL), mEmail, mTvEmail);

    final String facebook = mMapObject.getMetadata(Metadata.MetadataType.FMD_CONTACT_FACEBOOK);
    refreshMetadataOrHide(facebook, mFacebookPage, mTvFacebookPage);

    final String instagram = mMapObject.getMetadata(Metadata.MetadataType.FMD_CONTACT_INSTAGRAM);
    refreshMetadataOrHide(instagram, mInstagramPage, mTvInstagramPage);

    final String twitter = mMapObject.getMetadata(Metadata.MetadataType.FMD_CONTACT_TWITTER);
    refreshMetadataOrHide(twitter, mTwitterPage, mTvTwitterPage);

    final String vk = mMapObject.getMetadata(Metadata.MetadataType.FMD_CONTACT_VK);
    refreshMetadataOrHide(vk, mVkPage, mTvVkPage);

    final String line = mMapObject.getMetadata(Metadata.MetadataType.FMD_CONTACT_LINE);
    refreshMetadataOrHide(line, mLinePage, mTvLinePage);

    final String kayak = Config.isKayakDisplayEnabled() ? mMapObject.getKayakUrl() : null;
    UiUtils.showIf(!TextUtils.isEmpty(kayak), mKayak);
  }

  @Override
  public void onStart()
  {
    super.onStart();
    mViewModel.getMapObject().observe(requireActivity(), this);
  }

  @Override
  public void onStop()
  {
    super.onStop();
    mViewModel.getMapObject().removeObserver(this);
  }

  @Override
  public void onChanged(@Nullable  MapObject mapObject)
  {
    if (mapObject != null)
    {
      mMapObject = mapObject;
      refreshLinks();
    }
  }
}
