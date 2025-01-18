package app.organicmaps.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StyleableRes;

import app.organicmaps.R;
import app.organicmaps.sdk.routing.SingleLaneInfo;

public class LanesView extends View
{
  private interface DefaultValues
  {
    @ColorInt
    int BACKGROUND_COLOR = Color.BLACK;
    @ColorInt
    int ACTIVE_LANE_TINT_COLOR = Color.WHITE;
    @ColorInt
    int INACTIVE_LANE_TINT_COLOR = Color.GRAY;

    float CORNER_RADIUS = 0.0f;

    int LANES_COUNT = 5;
  }

  private final int mCornerRadius;

  @ColorInt
  private final int mActiveLaneTintColor;
  @ColorInt
  private final int mInactiveLaneTintColor;

  @NonNull
  private final Paint mBackgroundPaint;

  @Nullable
  private LanesDrawable mLanesDrawable;
  @Nullable
  private Rect mViewBounds = null;

  public LanesView(Context context, @Nullable AttributeSet attrs)
  {
    super(context, attrs);

    int backgroundColor;

    try (TypedArray data = context.getTheme().obtainStyledAttributes(attrs, R.styleable.LanesView, 0, 0))
    {
      backgroundColor = getAttrColor(data, R.styleable.LanesView_backgroundColor, DefaultValues.BACKGROUND_COLOR);
      mActiveLaneTintColor = getAttrColor(data, R.styleable.LanesView_activeLaneTintColor, DefaultValues.ACTIVE_LANE_TINT_COLOR);
      mInactiveLaneTintColor = getAttrColor(data, R.styleable.LanesView_inactiveLaneTintColor, DefaultValues.INACTIVE_LANE_TINT_COLOR);
      mCornerRadius = (int) Math.max(data.getDimension(R.styleable.LanesView_cornerRadius, DefaultValues.CORNER_RADIUS), 0.0f);

      if (isInEditMode())
      {
        final int lanesCount = Math.max(1, data.getInt(R.styleable.LanesView_editModeLanesCount, DefaultValues.LANES_COUNT));
        createLanesForEditMode(lanesCount);
      }
    }

    mBackgroundPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    mBackgroundPaint.setColor(backgroundColor);
  }

  public void setLanes(@Nullable SingleLaneInfo[] lanes)
  {
    if (lanes == null || lanes.length == 0)
      mLanesDrawable = null;
    else
      mLanesDrawable = new LanesDrawable(getContext(), lanes, mActiveLaneTintColor, mInactiveLaneTintColor);
    update();
  }

  @Override
  public void draw(@NonNull Canvas canvas)
  {
    super.draw(canvas);

    if (mLanesDrawable == null)
      return;

    final int paddingStart = getPaddingStart();
    final int paddingTop = getPaddingTop();
    final int paddingEnd = getPaddingEnd();
    final int paddingBottom = getPaddingBottom();

    mLanesDrawable.setBounds(paddingStart, paddingTop, getWidth() - paddingEnd, getHeight() - paddingBottom);

    mViewBounds = new Rect(mLanesDrawable.getBounds());

    mViewBounds.left -= paddingStart;
    mViewBounds.top -= paddingTop;
    mViewBounds.right += paddingEnd;
    mViewBounds.bottom += paddingBottom;

    canvas.drawRoundRect(new RectF(mViewBounds), mCornerRadius, mCornerRadius, mBackgroundPaint);

    mLanesDrawable.draw(canvas);
  }

  @Override
  public boolean onTouchEvent(MotionEvent event)
  {
    if (mViewBounds != null && mViewBounds.contains((int) event.getX(), (int) event.getY()))
    {
      performClick();
      return true;
    }

    return false;
  }

  @Override
  public boolean performClick()
  {
    super.performClick();
    return false;
  }

  private void update()
  {
    if (mLanesDrawable != null)
      setVisibility(VISIBLE);
    else
      setVisibility(GONE);
    invalidate();
  }

  @ColorInt
  private int getAttrColor(@NonNull TypedArray data, @StyleableRes int index, @ColorInt int defaultColor)
  {
    final TypedValue typedValue = new TypedValue();
    data.getValue(index, typedValue);
    if (typedValue.type == TypedValue.TYPE_ATTRIBUTE)
    {
      getContext().getTheme().resolveAttribute(typedValue.data, typedValue, true);
      return typedValue.data;
    }
    else
      return data.getColor(index, defaultColor);
  }

  private void createLanesForEditMode(int lanesCount)
  {
    final SingleLaneInfo[] lanes = new SingleLaneInfo[lanesCount];
    lanes[0] = new SingleLaneInfo(new byte[]{1}, false);
    if (lanes.length > 1)
      lanes[1] = new SingleLaneInfo(new byte[]{3}, false);
    for (int i = 2; i <= lanes.length - 1; i++)
      lanes[i] = new SingleLaneInfo(new byte[]{0}, true);
    if (lanes.length > 2)
      lanes[lanes.length - 2] = new SingleLaneInfo(new byte[]{8}, false);
    if (lanes.length > 3)
      lanes[lanes.length - 1] = new SingleLaneInfo(new byte[]{9}, false);

    setLanes(lanes);
  }
}
