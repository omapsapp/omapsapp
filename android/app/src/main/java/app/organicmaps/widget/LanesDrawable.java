package app.organicmaps.widget;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.content.ContextCompat;

import app.organicmaps.R;
import app.organicmaps.sdk.routing.SingleLaneInfo;

import java.util.Objects;

public class LanesDrawable extends Drawable
{
  @ColorRes
  private static final int ACTIVE_LANE_TINT_RES = R.color.white_primary;
  @ColorRes
  private static final int INACTIVE_LANE_TINT_RES = R.color.white_38;

  private static class TintColorInfo
  {
    @ColorInt
    public final int mActiveLaneTint;
    @ColorInt
    public final int mInactiveLaneTint;

    public TintColorInfo(@ColorInt int activeLaneTint, @ColorInt int inactiveLaneTint)
    {
      mActiveLaneTint = activeLaneTint;
      mInactiveLaneTint = inactiveLaneTint;
    }
  }

  private static class LaneDrawable
  {
    private final Drawable mDrawable;

    private LaneDrawable(@NonNull final Context context, @NonNull SingleLaneInfo laneInfo, int horizontalOffset, TintColorInfo colorInfo)
    {
      mDrawable = Objects.requireNonNull(AppCompatResources.getDrawable(context, laneInfo.mLane[0].mTurnRes));

      final int width = mDrawable.getIntrinsicWidth();
      final int height = mDrawable.getIntrinsicHeight();

      mDrawable.setBounds(horizontalOffset, 0, horizontalOffset + width, height);
      mDrawable.setTint(laneInfo.mIsActive ? colorInfo.mActiveLaneTint : colorInfo.mInactiveLaneTint);
    }

    private void draw(@NonNull final Canvas canvas)
    {
      mDrawable.draw(canvas);
    }
  }

  @NonNull
  private final LaneDrawable[] mLanes;

  private int mWidth;
  private int mHeight;

  public LanesDrawable(@NonNull final Context context, @NonNull SingleLaneInfo[] lanes)
  {
    final TintColorInfo tintColorInfo = new TintColorInfo(
        ContextCompat.getColor(context, ACTIVE_LANE_TINT_RES),
        ContextCompat.getColor(context, INACTIVE_LANE_TINT_RES));
    mLanes = createLaneDrawables(context, lanes, tintColorInfo);
  }

  public LanesDrawable(@NonNull final Context context, @NonNull SingleLaneInfo[] lanes, @ColorInt int activeLaneTint, @ColorInt int inactiveLaneTint)
  {
    final TintColorInfo tintColorInfo = new TintColorInfo(activeLaneTint, inactiveLaneTint);
    mLanes = createLaneDrawables(context, lanes, tintColorInfo);
  }

  @Override
  public int getIntrinsicWidth()
  {
    return mWidth;
  }

  @Override
  public int getIntrinsicHeight()
  {
    return mHeight;
  }

  @Override
  public void setBounds(int left, int top, int right, int bottom)
  {
    final int width = right - left;
    final int height = bottom - top;
    final float widthRatio = (float) width / mWidth;
    final float heightRatio = (float) height / mHeight;
    final float ratio = Math.min(widthRatio, heightRatio);

    final float widthForOneLane = ((float) mWidth / mLanes.length) * ratio;
    final float heightForOneLane = mHeight * ratio;

    mWidth = (int) (widthForOneLane * mLanes.length);
    mHeight = (int) heightForOneLane;

    float offsetX = (float) Math.abs(mWidth - width) / 2 + left;
    float offsetY = (float) Math.abs(mHeight - height) / 2 + top;
    for (final LaneDrawable drawable : mLanes)
    {
      final Rect bounds = drawable.mDrawable.getBounds();
      bounds.offsetTo((int) offsetX, (int) offsetY);
      bounds.right = (int) (bounds.left + widthForOneLane);
      bounds.bottom = (int) (bounds.top + heightForOneLane);
      offsetX += widthForOneLane;
    }

    super.setBounds(
        mLanes[0].mDrawable.getBounds().left,
        mLanes[0].mDrawable.getBounds().top,
        mLanes[mLanes.length - 1].mDrawable.getBounds().right,
        mLanes[0].mDrawable.getBounds().bottom);
  }

  @Override
  public void draw(@NonNull Canvas canvas)
  {
    for (final LaneDrawable drawable : mLanes)
      drawable.draw(canvas);
  }

  @Override
  public void setAlpha(int alpha) {}

  @Override
  public void setColorFilter(@Nullable ColorFilter colorFilter) {}

  @Override
  public int getOpacity()
  {
    return PixelFormat.UNKNOWN;
  }

  @NonNull
  private LaneDrawable[] createLaneDrawables(@NonNull Context context, @NonNull SingleLaneInfo[] lanes, @NonNull TintColorInfo tintColorInfo)
  {
    assert lanes.length > 0;

    final LaneDrawable[] laneDrawables = new LaneDrawable[lanes.length];

    int totalWidth = 0;
    for (int i = 0; i < lanes.length; ++i)
    {
      laneDrawables[i] = new LaneDrawable(context, lanes[i], totalWidth, tintColorInfo);
      totalWidth += laneDrawables[i].mDrawable.getBounds().width();
    }

    mWidth = totalWidth;
    mHeight = laneDrawables[0].mDrawable.getBounds().height();
    return laneDrawables;
  }
}
