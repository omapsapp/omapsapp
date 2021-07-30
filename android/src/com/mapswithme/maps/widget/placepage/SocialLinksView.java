package com.mapswithme.maps.widget.placepage;

import android.content.Context;
import android.os.Build;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.RelativeLayout;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;
import com.mapswithme.maps.R;
import com.mapswithme.util.Utils;
import com.mapswithme.util.log.Logger;
import com.mapswithme.util.log.LoggerFactory;

import java.util.ArrayList;
import java.util.List;

public class SocialLinksView extends RelativeLayout implements View.OnClickListener, View.OnLongClickListener
{
  private static final Logger LOGGER = LoggerFactory.INSTANCE.getLogger(LoggerFactory.Type.MISC);
  private static final String TAG = SocialLinksView.class.getSimpleName();

  private String mFacebookPageLink;
  private String mInstagramPageLink;
  private String mTwitterPageLink;
  private String mVkPageLink;

  private PlacePageView mPlacePageView;

  private ImageView mFacebookIcon;
  private ImageView mInstagramIcon;
  private ImageView mTwitterIcon;
  private ImageView mVkIcon;

  public SocialLinksView(@NonNull Context context)
  {
    super(context);
    LayoutInflater.from(getContext()).inflate(R.layout.place_page_social_links, this, true);
  }

  public SocialLinksView(Context context, AttributeSet attrs)
  {
    super(context, attrs);
    LayoutInflater.from(getContext()).inflate(R.layout.place_page_social_links, this, true);
  }

  public SocialLinksView(Context context, AttributeSet attrs, int defStyleAttr)
  {
    super(context, attrs, defStyleAttr);
    LayoutInflater.from(getContext()).inflate(R.layout.place_page_social_links, this, true);
  }

  @RequiresApi(api = Build.VERSION_CODES.LOLLIPOP)
  public SocialLinksView(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes)
  {
    super(context, attrs, defStyleAttr, defStyleRes);
    LayoutInflater.from(getContext()).inflate(R.layout.place_page_social_links, this, true);
  }

  @Override
  protected void onFinishInflate()
  {
    super.onFinishInflate();
    mFacebookIcon = findViewById(R.id.iv__place_facebook);
    mFacebookIcon.setOnClickListener(this);
    mFacebookIcon.setOnLongClickListener(this);
    mInstagramIcon = findViewById(R.id.iv__place_instagram);
    mInstagramIcon.setOnClickListener(this);
    mInstagramIcon.setOnLongClickListener(this);
    mTwitterIcon = findViewById(R.id.iv__place_twitter);
    mTwitterIcon.setOnClickListener(this);
    mTwitterIcon.setOnLongClickListener(this);
    mVkIcon = findViewById(R.id.iv__place_vk);
    mVkIcon.setOnClickListener(this);
    mVkIcon.setOnLongClickListener(this);
  }

  public void setPlacePageView(PlacePageView placePageView)
  {
    mPlacePageView = placePageView;
  }

  public void refreshSocialLinks(String facebookPageLink, String instagramPageLink,
                                 String twitterPageLink, String vkPageLink)
  {
    this.mFacebookPageLink = facebookPageLink;
    this.mInstagramPageLink = instagramPageLink;
    this.mTwitterPageLink = twitterPageLink;
    this.mVkPageLink = vkPageLink;

    final boolean hasSocialLinks = !TextUtils.isEmpty(facebookPageLink) ||
                                   !TextUtils.isEmpty(instagramPageLink) ||
                                   !TextUtils.isEmpty(twitterPageLink) ||
                                   !TextUtils.isEmpty(vkPageLink);

    mFacebookIcon.setVisibility(TextUtils.isEmpty(facebookPageLink) ? GONE : VISIBLE);
    mInstagramIcon.setVisibility(TextUtils.isEmpty(instagramPageLink) ? GONE : VISIBLE);
    mTwitterIcon.setVisibility(TextUtils.isEmpty(twitterPageLink) ? GONE : VISIBLE);
    mVkIcon.setVisibility(TextUtils.isEmpty(vkPageLink) ? GONE : VISIBLE);

    this.setVisibility(hasSocialLinks ? VISIBLE : GONE);
  }

  @Override
  public void onClick(View view)
  {
    switch(view.getId())
    {
      case R.id.iv__place_facebook:
        if (TextUtils.isEmpty(mFacebookPageLink))
        {
          LOGGER.e(TAG, "Cannot follow facebook url");
          break;
        }
        Utils.openUrl(getContext(), getFullUrl(mFacebookPageLink, "facebook.com"));
        break;
      case R.id.iv__place_instagram:
        if (TextUtils.isEmpty(mInstagramPageLink))
        {
          LOGGER.e(TAG, "Cannot follow instagram url");
          break;
        }
        Utils.openUrl(getContext(), getFullUrl(mInstagramPageLink, "instagram.com"));
        break;
      case R.id.iv__place_twitter:
        if (TextUtils.isEmpty(mTwitterPageLink))
        {
          LOGGER.e(TAG, "Cannot follow twitter url");
          break;
        }
        Utils.openUrl(getContext(), getFullUrl(mTwitterPageLink, "twitter.com"));
        break;
      case R.id.iv__place_vk:
        if (TextUtils.isEmpty(mVkPageLink))
        {
          LOGGER.e(TAG, "Cannot follow vk url");
          break;
        }
        Utils.openUrl(getContext(), mVkPageLink);
        break;
    }

  }

  @Override
  public boolean onLongClick(View view)
  {
    if (mPlacePageView == null)
      return false; //Can't show menu without mPlacePageView

    final List<String> items = new ArrayList<>();

    switch(view.getId())
    {
      case R.id.iv__place_facebook:
        if (TextUtils.isEmpty(mFacebookPageLink))
        {
          LOGGER.e(TAG, "A long click tap on facebook icon cannot be handled");
          break;
        }
        items.add(getFullUrl(mFacebookPageLink, "facebook.com"));
        break;
      case R.id.iv__place_instagram:
        if (TextUtils.isEmpty(mInstagramPageLink))
        {
          LOGGER.e(TAG, "A long click tap on instagram icon cannot be handled");
          break;
        }
        items.add(getFullUrl(mInstagramPageLink, "instagram.com"));
        break;
      case R.id.iv__place_twitter:
        if (TextUtils.isEmpty(mTwitterPageLink))
        {
          LOGGER.e(TAG, "A long click tap on twitter icon cannot be handled");
          break;
        }
        items.add(getFullUrl(mTwitterPageLink, "twitter.com"));
        break;
      case R.id.iv__place_vk:
        if (TextUtils.isEmpty(mVkPageLink))
        {
          LOGGER.e(TAG, "A long click tap on vk icon cannot be handled");
          break;
        }
        items.add(mVkPageLink);
        break;
      default:
        return false;
    }

    mPlacePageView.showCopyMenu(view, items);
    return true;
  }

  private String getFullUrl(String socialPage, String domain)
  {
    //1. socialPage contains full URL
    if (socialPage.startsWith("http://") || socialPage.startsWith("https://"))
      return socialPage;

    //2. socialPage contains full URL without schema
    if (socialPage.contains(domain))
      return "https://" + socialPage;

    //3. socialPage contains username
    return "https://" + domain + "/" + socialPage;
  }
}
