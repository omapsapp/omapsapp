package com.mapswithme.maps.purchase;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.mapswithme.util.log.Logger;
import com.mapswithme.util.log.LoggerFactory;

abstract class AbstractBookmarkValidationCallback implements ValidationCallback
{
  private static final Logger LOGGER = LoggerFactory.INSTANCE.getLogger(LoggerFactory.Type.BILLING);
  private static final String TAG = AbstractBookmarkValidationCallback.class.getSimpleName();
  @Nullable
  private final String mServerId;

  AbstractBookmarkValidationCallback(@Nullable String serverId)
  {
    mServerId = serverId;
  }

  @Override
  public final void onValidate(@NonNull String purchaseData, @NonNull ValidationStatus status,
                               boolean isTrial)
  {
    LOGGER.i(TAG, "Validation status of 'paid bookmark': " + status);
    if (status == ValidationStatus.VERIFIED)
    {
      consumePurchase(purchaseData);
      return;
    }

    // We consume purchase in 'NOT_VERIFIED' case to allow user enter in bookmark catalog again.
    if (status == ValidationStatus.NOT_VERIFIED)
      consumePurchase(purchaseData);

    onValidationError(status);
  }

  abstract void onValidationError(@NonNull ValidationStatus status);

  abstract void consumePurchase(@NonNull String purchaseData);
}
