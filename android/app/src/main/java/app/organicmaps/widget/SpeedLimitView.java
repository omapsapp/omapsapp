package app.organicmaps.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.Typeface;
import android.util.AttributeSet;
import android.util.Pair;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;

import app.organicmaps.R;
import app.organicmaps.util.StringUtils;

public class SpeedLimitView extends View
{
  private interface DefaultValues
  {
    @ColorInt
    int BACKGROUND_COLOR = Color.WHITE;
    @ColorInt
    int TEXT_COLOR = Color.BLACK;
    @ColorInt
    int TEXT_ALERT_COLOR = Color.WHITE;

    float BORDER_WIDTH_RATIO = 0.1f;
  }

  @ColorInt
  private final int mBackgroundColor;

  @ColorInt
  private final int mBorderColor;

  @ColorInt
  private final int mAlertColor;

  @ColorInt
  private final int mTextColor;

  @ColorInt
  private final int mTextAlertColor;

  @NonNull
  private final Paint mSignBackgroundPaint;
  @NonNull
  private final Paint mSignBorderPaint;
  @NonNull
  private final Paint mTextPaint;

  private float mWidth;
  private float mHeight;
  private float mBackgroundRadius;
  private float mBorderRadius;
  private float mBorderWidth;

  private double mSpeedLimitMps;
  @Nullable
  private String mSpeedLimitStr;

  private double mCurrentSpeed;

  public SpeedLimitView(Context context, @Nullable AttributeSet attrs)
  {
    super(context, attrs);

    try (TypedArray data = context.getTheme()
                                  .obtainStyledAttributes(attrs, R.styleable.SpeedLimitView, 0, 0))
    {
      mBackgroundColor = data.getColor(R.styleable.SpeedLimitView_BackgroundColor, DefaultValues.BACKGROUND_COLOR);
      mBorderColor = data.getColor(R.styleable.SpeedLimitView_borderColor, ContextCompat.getColor(context, R.color.base_red));
      mAlertColor = data.getColor(R.styleable.SpeedLimitView_alertColor, ContextCompat.getColor(context, R.color.base_red));
      mTextColor = data.getColor(R.styleable.SpeedLimitView_textColor, DefaultValues.TEXT_COLOR);
      mTextAlertColor = data.getColor(R.styleable.SpeedLimitView_textAlertColor, DefaultValues.TEXT_ALERT_COLOR);
      if (isInEditMode())
      {
        mSpeedLimitMps = data.getInt(R.styleable.SpeedLimitView_editModeSpeedLimit, -1);
        mSpeedLimitStr = mSpeedLimitMps > 0 ? String.valueOf(((int) mSpeedLimitMps)) : null;
        mCurrentSpeed = data.getInt(R.styleable.SpeedLimitView_editModeCurrentSpeed, -1);
      }
    }

    mSignBackgroundPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    mSignBackgroundPaint.setColor(mBackgroundColor);

    mSignBorderPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    mSignBorderPaint.setColor(mBorderColor);
    mSignBorderPaint.setStrokeWidth(mBorderWidth);
    mSignBorderPaint.setStyle(Paint.Style.STROKE);

    mTextPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    mTextPaint.setColor(mTextColor);
    mTextPaint.setTextAlign(Paint.Align.CENTER);
    mTextPaint.setTypeface(Typeface.create(Typeface.DEFAULT, Typeface.BOLD));
  }

  public void setSpeedLimitMps(final double speedLimitMps)
  {
    if (mSpeedLimitMps == speedLimitMps)
      return;

    mSpeedLimitMps = speedLimitMps;
    if (mSpeedLimitMps <= 0)
    {
      mSpeedLimitStr = null;
      setVisibility(GONE);
      return;
    }

    final Pair<String, String> speedLimitAndUnits = StringUtils.nativeFormatSpeedAndUnits(mSpeedLimitMps);
    setVisibility(VISIBLE);
    mSpeedLimitStr = speedLimitAndUnits.first;
    configureTextSize();
    invalidate();
  }

  public void setCurrentSpeed(final double currentSpeed)
  {
    mCurrentSpeed = currentSpeed;
    invalidate();
  }

  @Override
  protected void onDraw(@NonNull Canvas canvas)
  {
    super.onDraw(canvas);

    final boolean alert = mCurrentSpeed > mSpeedLimitMps && mSpeedLimitMps > 0;

    final float cx = mWidth / 2;
    final float cy = mHeight / 2;

    drawSign(canvas, cx, cy, alert);
    drawText(canvas, cx, cy, alert);
  }

  private void drawSign(@NonNull Canvas canvas, float cx, float cy, boolean alert)
  {
    if (alert)
      mSignBackgroundPaint.setColor(mAlertColor);
    else
      mSignBackgroundPaint.setColor(mBackgroundColor);

    canvas.drawCircle(cx, cy, mBackgroundRadius, mSignBackgroundPaint);
    if (!alert)
    {
      mSignBorderPaint.setStrokeWidth(mBorderWidth);
      canvas.drawCircle(cx, cy, mBorderRadius, mSignBorderPaint);
    }
  }

  private void drawText(@NonNull Canvas canvas, float cx, float cy, boolean alert)
  {
    if (mSpeedLimitStr == null)
      return;

    if (alert)
      mTextPaint.setColor(mTextAlertColor);
    else
      mTextPaint.setColor(mTextColor);

    final Rect textBounds = new Rect();
    mTextPaint.getTextBounds(mSpeedLimitStr, 0, mSpeedLimitStr.length(), textBounds);
    final float textY = cy - textBounds.exactCenterY();
    canvas.drawText(mSpeedLimitStr, cx, textY, mTextPaint);
  }

  @Override
  protected void onSizeChanged(int w, int h, int oldw, int oldh)
  {
    super.onSizeChanged(w, h, oldw, oldh);

    final float paddingX = (float) (getPaddingLeft() + getPaddingRight());
    final float paddingY = (float) (getPaddingTop() + getPaddingBottom());

    mWidth = (float) w - paddingX;
    mHeight = (float) h - paddingY;
    mBackgroundRadius = Math.min(mWidth, mHeight) / 2;
    mBorderWidth = mBackgroundRadius * 2 * DefaultValues.BORDER_WIDTH_RATIO;
    mBorderRadius = mBackgroundRadius - mBorderWidth / 2;
    configureTextSize();
  }

  // Apply binary search to determine the optimal text size that fits within the circular boundary.
  private void configureTextSize()
  {
    if (mSpeedLimitStr == null)
      return;

    final String text = mSpeedLimitStr;
    final float textRadius = mBorderRadius - mBorderWidth;
    final float textMaxSize = 2 * textRadius;
    final float textMaxSizeSquared = (float) Math.pow(textMaxSize, 2);

    float lowerBound = 0;
    float upperBound = textMaxSize;
    float textSize = textMaxSize;
    final Rect textBounds = new Rect();

    while (lowerBound <= upperBound)
    {
      textSize = (lowerBound + upperBound) / 2;
      mTextPaint.setTextSize(textSize);
      mTextPaint.getTextBounds(text, 0, text.length(), textBounds);

      if (Math.pow(textBounds.width(), 2) + Math.pow(textBounds.height(), 2) <= textMaxSizeSquared)
        lowerBound = textSize + 1;
      else
        upperBound = textSize - 1;
    }

    mTextPaint.setTextSize(Math.max(1, textSize));
  }
}
