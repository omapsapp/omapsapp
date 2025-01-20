package app.organicmaps.widget;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.util.AttributeSet;
import android.util.Pair;
import android.view.View;
import android.view.WindowInsets;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;

import app.organicmaps.Framework;
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
    int UNLIMITED_BORDER_COLOR = Color.BLACK;
    @ColorInt
    int TEXT_ALERT_COLOR = Color.WHITE;

    float BORDER_WIDTH_RATIO = 0.2f;

    float BORDER_RADIUS_RATIO = 0.95f;
  }

  @ColorInt
  private final int mBackgroundColor;

  @ColorInt
  private final int mBackgroundColorWidget;

  @ColorInt
  private final int mBorderColor;

  @ColorInt
  private final int mUnlimitedBorderColor;

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
  private final Paint mSignUnlimitedBorderPaint;
  @NonNull
  private final Paint mTextPaint;
  @NonNull
  private final Paint mCurrentSpeedBorderPaint;
  @NonNull
  private final Paint mCurrentSpeedBackgroundPaint;
  @NonNull
  private final Paint mCurrentSpeedTextPaint;
  @NonNull
  private final Paint mCurrentSpeedUnitsTextPaint; // Added for units text
  @NonNull
  private final Paint mBackgroundPaint;
  @NonNull
  private final Paint mWidgetBackgroundPaint;
  private float mSpeedLimitAlpha;


  private float mWidth;
  private float mHeight;
  private float mBackgroundRadius;
  private float mBorderRadius;
  private float mBorderWidth;

  private double mSpeedLimitMps;
  @Nullable
  private String mSpeedLimitStr;
  @Nullable
  private String mCurrentSpeedStr;
  @Nullable
  private String mCurrentSpeedUnitsStr;

  private double mCurrentSpeedMps;

  /**
   * We’ll store the bottom system navigation bar (or gesture bar) inset here.
   * Instead of subtracting it from the height, we’ll apply a vertical translation
   * so the entire widget moves up.
   */
  private int mBottomSystemWindowInset = 0;
  private int mLeftSystemWindowInset = 0;

  @SuppressLint("SwitchIntDef")
  public SpeedLimitView(Context context, @Nullable AttributeSet attrs)
  {
    super(context, attrs);

    try (TypedArray data = context.getTheme()
            .obtainStyledAttributes(attrs, R.styleable.SpeedLimitView, 0, 0))
    {
      mBackgroundColorWidget = data.getColor(R.styleable.SpeedLimitView_WidgetBackgroundColor, switch (Framework.nativeGetMapStyle()) {
        case Framework.MAP_STYLE_DARK, Framework.MAP_STYLE_VEHICLE_DARK, Framework.MAP_STYLE_OUTDOORS_DARK ->
                Color.DKGRAY;
        default -> Color.WHITE;
      });
      mBackgroundColor = data.getColor(R.styleable.SpeedLimitView_BackgroundColor, DefaultValues.BACKGROUND_COLOR);
      mBorderColor = data.getColor(R.styleable.SpeedLimitView_borderColor, ContextCompat.getColor(context, R.color.base_red));
      mUnlimitedBorderColor = data.getColor(R.styleable.SpeedLimitView_unlimitedBorderColor, DefaultValues.UNLIMITED_BORDER_COLOR);
      mAlertColor = data.getColor(R.styleable.SpeedLimitView_alertColor, ContextCompat.getColor(context, R.color.base_red));
      mTextColor = data.getColor(R.styleable.SpeedLimitView_textColor, DefaultValues.TEXT_COLOR);
      mTextAlertColor = data.getColor(R.styleable.SpeedLimitView_textAlertColor, DefaultValues.TEXT_ALERT_COLOR);
      if (isInEditMode())
      {
        mSpeedLimitMps = data.getInt(R.styleable.SpeedLimitView_editModeSpeedLimit, -1);
        mSpeedLimitStr = mSpeedLimitMps > 0 ? String.valueOf(((int) mSpeedLimitMps)) : null;
        mCurrentSpeedMps = data.getInt(R.styleable.SpeedLimitView_editModeCurrentSpeed, -1);
        mCurrentSpeedStr = mCurrentSpeedMps > 0 ? String.valueOf(((int) mCurrentSpeedMps)) : null;
      }
    }

    mSignBackgroundPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    mSignBackgroundPaint.setColor(mBackgroundColor);

    mSignBorderPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    mSignBorderPaint.setColor(mBorderColor);
    mSignBorderPaint.setStrokeWidth(mBorderWidth);
    mSignBorderPaint.setStyle(Paint.Style.STROKE);
    mSignUnlimitedBorderPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    mSignUnlimitedBorderPaint.setColor(mUnlimitedBorderColor);
    mSignUnlimitedBorderPaint.setStrokeWidth(mBorderWidth);
    mSignUnlimitedBorderPaint.setStyle(Paint.Style.STROKE);

    mTextPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    mTextPaint.setColor(mTextColor);
    mTextPaint.setTextAlign(Paint.Align.CENTER);
    mTextPaint.setTypeface(Typeface.create(Typeface.DEFAULT, Typeface.BOLD));

    mCurrentSpeedBackgroundPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    mCurrentSpeedBackgroundPaint.setColor(mBackgroundColor);
    mCurrentSpeedTextPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    mCurrentSpeedTextPaint.setColor(mTextColor);
    mCurrentSpeedTextPaint.setTextAlign(Paint.Align.CENTER);
    mCurrentSpeedTextPaint.setTypeface(Typeface.create(Typeface.DEFAULT, Typeface.BOLD));
    mCurrentSpeedUnitsTextPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    mCurrentSpeedUnitsTextPaint.setColor(mTextColor);
    mCurrentSpeedUnitsTextPaint.setTextAlign(Paint.Align.CENTER);
    mCurrentSpeedUnitsTextPaint.setTypeface(Typeface.create(Typeface.DEFAULT, Typeface.NORMAL));
    mCurrentSpeedBorderPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    mCurrentSpeedBorderPaint.setColor(Color.BLACK);
    mCurrentSpeedBorderPaint.setStyle(Paint.Style.STROKE);
    mBackgroundPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    mBackgroundPaint.setColor(mBackgroundColor);
    mWidgetBackgroundPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    mWidgetBackgroundPaint.setColor(mBackgroundColorWidget);
    mSpeedLimitMps = -1.0;
  }

  /**
   * Capture bottom navigation bar insets (gesture area).
   * We’ll use this to translate the canvas up.
   */
  @Override
  @NonNull
  public WindowInsets onApplyWindowInsets(@NonNull WindowInsets insets)
  {
    mBottomSystemWindowInset = insets.getSystemWindowInsetBottom();
    mLeftSystemWindowInset = insets.getSystemWindowInsetLeft();
    // Request a re-draw so we can apply the translation in onDraw().
    invalidate();
    return super.onApplyWindowInsets(insets);
  }

  public void setSpeedLimitMps(final double speedLimitMps)
  {
    if (mSpeedLimitMps == speedLimitMps)
      return;

    boolean hadSpeedLimit = mSpeedLimitMps >= 0;
    mSpeedLimitMps = speedLimitMps;
    boolean hasSpeedLimit = mSpeedLimitMps >= 0;

    if (mSpeedLimitMps >= 0) {
      final Pair<String, String> speedLimitAndUnits = StringUtils.nativeFormatSpeedAndUnits(mSpeedLimitMps);
      mSpeedLimitStr = speedLimitAndUnits.first;
    }

    if (hadSpeedLimit != hasSpeedLimit) {
      animateSpeedLimitAlpha(hasSpeedLimit);
    } else {
      invalidate();
    }
    configureTextSize();
    invalidate();
  }

  private void animateSpeedLimitAlpha(final boolean fadeIn) {
    float startAlpha = fadeIn ? 0f : 1f;
    float endAlpha = fadeIn ? 1f : 0f;
    ValueAnimator animator = ValueAnimator.ofFloat(startAlpha, endAlpha);
    animator.setDuration(500);
    animator.addUpdateListener(animation -> {
      mSpeedLimitAlpha = (float) animation.getAnimatedValue();
      invalidate();
    });
    animator.addListener(new AnimatorListenerAdapter() {
      @Override
      public void onAnimationEnd(Animator animation) {
        if (!fadeIn) {
          mSpeedLimitStr = null;
        }
      }
    });
    animator.start();
  }
  public void setCurrentSpeed(final double currentSpeedMps) {
    mCurrentSpeedMps = currentSpeedMps;
    final Pair<String, String> speedAndUnits = StringUtils.nativeFormatSpeedAndUnits(mCurrentSpeedMps);
    if (mCurrentSpeedMps < 0)
      mCurrentSpeedStr = "0";
    else
      mCurrentSpeedStr = speedAndUnits.first;
    mCurrentSpeedUnitsStr = speedAndUnits.second;
    configureTextSize();
    invalidate();
  }

  private void draw_widgets(@NonNull Canvas canvas, float cx, float cx_or_cy, float cy, boolean isLandscape) {
    final boolean alert = mSpeedLimitMps > 0 && mSpeedLimitStr != null && Integer.parseInt(mCurrentSpeedStr) > Integer.parseInt(mSpeedLimitStr); //TODO do better?
    float second_cx = (isLandscape) ? cx_or_cy : cx;
    float second_cy = (isLandscape) ? cy : cx_or_cy;
    // Draw combined background and speed limit sign with layer alpha
    if (mSpeedLimitAlpha > 0f) {
      int saveCount = canvas.saveLayerAlpha(0, 0, getWidth(), getHeight(), (int) (mSpeedLimitAlpha * 255));
      drawCombinedBackground(canvas, cx, cx_or_cy, cy);
      // Draw speed limit sign and text
      if (mSpeedLimitStr != null)
        drawSpeedLimitSign(canvas, cx, second_cy);

      canvas.restoreToCount(saveCount);
    }
    // Draw current speed sign and text (always visible)
    if (mCurrentSpeedStr != null)
      drawCurrentSpeedSign(canvas, second_cx, cy, alert);
  }

  @Override
  protected void onDraw(@NonNull Canvas canvas) {
    super.onDraw(canvas);
    canvas.save();
    canvas.translate(mLeftSystemWindowInset,  -mBottomSystemWindowInset);
    final boolean isLandscape = mWidth > mHeight;

    if (isLandscape) {
      final float cy = mHeight / 2;
      float gap = mWidth * 0.02f;
      final float totalWidth = 4 * mBackgroundRadius + gap;
      final float startX = (mWidth - totalWidth) / 2;
      final float currentSpeedCx = startX + mBackgroundRadius;
      final float speedLimitCx = currentSpeedCx + 2 * mBackgroundRadius + gap;
      draw_widgets(canvas, speedLimitCx, currentSpeedCx, cy, true);
    } else {
      final float cx = mWidth / 2;
      float gap = mHeight * 0.02f;
      final float totalHeight = 4 * mBackgroundRadius + gap;
      final float startY = (mHeight - totalHeight) / 2;
      final float speedLimitCy = startY + mBackgroundRadius;
      final float currentSpeedCy = speedLimitCy + 2 * mBackgroundRadius + gap;
      draw_widgets(canvas, cx, speedLimitCy, currentSpeedCy, false);
    }
  }

  private void drawCombinedBackground(Canvas canvas, float cx, float cx_or_cy, float cy) {
    final boolean isLandscape = mWidth > mHeight;
    float left;
    float right;
    float top;
    float bottom;

    if (isLandscape) {
      top = cy - mBackgroundRadius;
      bottom = cy + mBackgroundRadius;
      left = cx_or_cy - mBackgroundRadius;
      right = cx + mBackgroundRadius;
    } else {
      left = cx - mBackgroundRadius;
      right = cx + mBackgroundRadius;
      top = cx_or_cy - mBackgroundRadius;
      bottom = cy + mBackgroundRadius;
    }
    RectF rect = new RectF(left, top, right, bottom);
    float cornerRadius = mBackgroundRadius;
    canvas.drawRoundRect(rect, cornerRadius, cornerRadius, mWidgetBackgroundPaint);
  }

  private void drawUnlimitedSign(@NonNull Canvas canvas, float cx, float cy) {
    // Paint for stripes
    Paint stripesPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    stripesPaint.setColor(Color.BLACK);
    stripesPaint.setStyle(Paint.Style.STROKE);

    // Thickness of the stripes can be relative to the border width or radius
    float stripeThickness = mBorderWidth * 0.4f;
    stripesPaint.setStrokeWidth(stripeThickness);

    float r = mBackgroundRadius;

    // Direction vector for the diagonal lines (top-right to bottom-left)
    float dx = -1.0f / (float)Math.sqrt(2);
    float dy =  1.0f / (float)Math.sqrt(2);

    // Perpendicular vector to D (rotate by +90°)
    float px = -dy;
    float py = dx;

    // Increase lineSpacing to spread stripes further apart
    float lineSpacing = r * 0.12f; // slightly larger than 0.1f

    // Use a scale factor to shorten the lines
    float lineScale = 0.8f; // reduce from full radius to 85% radius

    int[] offsets = {-2, -1, 0, 1, 2};

    // Draw 5 parallel stripes
    for (int i : offsets) {
      float ox = i * lineSpacing * px;
      float oy = i * lineSpacing * py;

      // Start and end points of the line, shortened by lineScale
      float sx = cx + dx * r * lineScale + ox;
      float sy = cy + dy * r * lineScale + oy;
      float ex = cx - dx * r * lineScale + ox;
      float ey = cy - dy * r * lineScale + oy;

      canvas.drawLine(sx, sy, ex, ey, stripesPaint);
    }
  }


  private void drawSpeedLimitSign(@NonNull Canvas canvas, float cx, float cy) {
    // Draw the sign background and border as before
    mSignBackgroundPaint.setColor(mBackgroundColor);
    canvas.drawCircle(cx, cy, mBackgroundRadius, mSignBackgroundPaint);

    if (mSpeedLimitMps == 0) {
      // Unlimited speed scenario: Draw the special no-limit pattern
      mSignUnlimitedBorderPaint.setStrokeWidth(mBorderWidth);
      canvas.drawCircle(cx, cy, mBorderRadius * DefaultValues.BORDER_RADIUS_RATIO, mSignUnlimitedBorderPaint);
      drawUnlimitedSign(canvas, cx, cy);
    }
    else if (mSpeedLimitStr != null && mSpeedLimitMps > 0) {
      // Normal speed limit scenario: Draw the text
      mSignBorderPaint.setStrokeWidth(mBorderWidth);
      canvas.drawCircle(cx, cy, mBorderRadius * DefaultValues.BORDER_RADIUS_RATIO, mSignBorderPaint);
      drawSpeedLimitText(canvas, cx, cy);
    }
  }

  private void drawSpeedLimitText(@NonNull Canvas canvas, float cx, float cy)
  {
    if (mSpeedLimitStr == null)
      return;

    mTextPaint.setColor(mTextColor);

    final Rect textBounds = new Rect();
    mTextPaint.getTextBounds(mSpeedLimitStr, 0, mSpeedLimitStr.length(), textBounds);
    final float textY = cy - textBounds.exactCenterY();
    canvas.drawText(mSpeedLimitStr, cx, textY, mTextPaint);
  }

  private void drawCurrentSpeedSign(@NonNull Canvas canvas, float cx, float cy, boolean alert) {
    // Change background color based on alert state
    mCurrentSpeedBackgroundPaint.setColor(mBackgroundColor);
    // Draw current speed circle (background)
    canvas.drawCircle(cx, cy, mBackgroundRadius, mCurrentSpeedBackgroundPaint);
    // Draw border around current speed circle
    if (alert) {
      mCurrentSpeedBackgroundPaint.setColor(mAlertColor);
      canvas.drawCircle(cx, cy, mBackgroundRadius * DefaultValues.BORDER_RADIUS_RATIO * 0.95f, mCurrentSpeedBackgroundPaint);
    }
    mCurrentSpeedBorderPaint.setStrokeWidth(mBorderWidth / 2);
    canvas.drawCircle(cx, cy, mBorderRadius, mCurrentSpeedBorderPaint);
    drawCurrentSpeedText(canvas, cx, cy, alert);
  }

  private void drawCurrentSpeedText(@NonNull Canvas canvas, float cx, float cy, boolean alert) {
    if (mCurrentSpeedStr == null || mCurrentSpeedUnitsStr == null)
      return;
    // Change text color based on alert state
    if (alert) {
      mCurrentSpeedTextPaint.setColor(mTextAlertColor);
      mCurrentSpeedUnitsTextPaint.setColor(mTextAlertColor);
    } else {
      mCurrentSpeedTextPaint.setColor(mTextColor);
      mCurrentSpeedUnitsTextPaint.setColor(mTextColor);
    }
    final Rect speedTextBounds = new Rect();
    mCurrentSpeedTextPaint.getTextBounds(mCurrentSpeedStr, 0, mCurrentSpeedStr.length(), speedTextBounds);
    final Rect unitsTextBounds = new Rect();
    mCurrentSpeedUnitsTextPaint.getTextBounds(mCurrentSpeedUnitsStr, 0, mCurrentSpeedUnitsStr.length(), unitsTextBounds);
    // Position speed text
    float speedTextY = cy - (float) speedTextBounds.height() / 2 + speedTextBounds.height();
    // Position units text
    float unitsTextY = speedTextY + unitsTextBounds.height() + 10f;
    // Draw speed text
    canvas.drawText(mCurrentSpeedStr, cx, speedTextY, mCurrentSpeedTextPaint);
    // Draw units text
    canvas.drawText(mCurrentSpeedUnitsStr, cx, unitsTextY, mCurrentSpeedUnitsTextPaint);
  }

  @Override
  protected void onSizeChanged(int w, int h, int oldw, int oldh)
  {
    super.onSizeChanged(w, h, oldw, oldh);

    final float paddingX = getPaddingLeft() + getPaddingRight();
    final float paddingY = getPaddingTop() + getPaddingBottom();
    mWidth = w - paddingX;
    mHeight = h - paddingY;

    boolean isLandscape = mWidth > mHeight;
    float gap = (isLandscape) ? mWidth * 0.1f : mHeight * 0.1f;
    // Compute maximum possible radius
    mBackgroundRadius = (isLandscape) ? (mWidth - gap) / 4 : (mHeight - gap) / 4;
    // Ensure the radius does not exceed half of the width
    mBackgroundRadius = (isLandscape) ? Math.min(mBackgroundRadius, mHeight / 2) : Math.min(mBackgroundRadius, mWidth / 2);
    mBorderWidth = mBackgroundRadius * DefaultValues.BORDER_WIDTH_RATIO;
    mBorderRadius = mBackgroundRadius - mBorderWidth / 2;
    configureTextSize();
  }

  // Apply binary search to determine the optimal text size that fits within the circular boundary.
  private void configureTextSize() {
    // Use reference strings to keep text size consistent
    configureTextSizeForString(mTextPaint); // For speed limit
    configureCurrentSpeedTextSize(); // For current speed and units
  }
  private void configureCurrentSpeedTextSize() {
    final float textRadius = mBorderRadius * 0.75f - mCurrentSpeedBorderPaint.getStrokeWidth();
    final float availableHeight = 2 * textRadius;
    float speedTextHeightRatio = 0.75f;
    float unitsTextHeightRatio = 0.2f;
    float speedTextHeight = availableHeight * speedTextHeightRatio;
    float unitsTextHeight = availableHeight * unitsTextHeightRatio;
    float speedTextSize = findMaxTextSizeForHeight("299", mCurrentSpeedTextPaint, speedTextHeight);
    float unitsTextSize = findMaxTextSizeForHeight("km/h", mCurrentSpeedUnitsTextPaint, unitsTextHeight);
    mCurrentSpeedTextPaint.setTextSize(speedTextSize);
    mCurrentSpeedUnitsTextPaint.setTextSize(unitsTextSize);
  }
  private float findMaxTextSizeForHeight(String text, Paint paint, float targetHeight) {
    float lowerBound = 0;
    float upperBound = targetHeight;
    float textSize = targetHeight;
    final Rect textBounds = new Rect();
    while (upperBound - lowerBound > 1f) {
      textSize = (lowerBound + upperBound) / 2;
      paint.setTextSize(textSize);
      paint.getTextBounds(text, 0, text.length(), textBounds);
      if (textBounds.height() <= targetHeight)
        lowerBound = textSize;
      else
        upperBound = textSize;
    }
    return lowerBound;
  }

  private void configureTextSizeForString(Paint textPaint) {
    final float textRadius = mBorderRadius - mBorderWidth;
    final float textMaxSize = 2 * textRadius;
    final float textMaxSizeSquared = textMaxSize * textMaxSize;

    float lowerBound = 0;
    float upperBound = textMaxSize;
    float textSize = textMaxSize;
    final Rect textBounds = new Rect();

    while (lowerBound <= upperBound)
    {
      textSize = (lowerBound + upperBound) / 2;
      textPaint.setTextSize(textSize);
      textPaint.getTextBounds("180", 0, 3, textBounds);

      if (textBounds.width() * textBounds.width() + textBounds.height() * textBounds.height() <= textMaxSizeSquared)
        lowerBound = textSize + 1;
      else
        upperBound = textSize - 1;
    }

    textPaint.setTextSize(Math.max(1, textSize));
  }
}