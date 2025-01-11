package app.organicmaps.maplayer;

import androidx.annotation.Nullable;
import androidx.lifecycle.MutableLiveData;
import androidx.lifecycle.ViewModel;
import app.organicmaps.location.TrackRecorder;

public class MapButtonsViewModel extends ViewModel
{
  private final MutableLiveData<Boolean> mButtonsHidden = new MutableLiveData<>(false);
  private final MutableLiveData<Float> mBottomButtonsHeight = new MutableLiveData<>(0f);
  private final MutableLiveData<Integer> mTopButtonsMarginTop = new MutableLiveData<>(-1);
  private final MutableLiveData<MapButtonsController.LayoutMode> mLayoutMode = new MutableLiveData<>(MapButtonsController.LayoutMode.regular);
  private final MutableLiveData<Integer> mMyPositionMode = new MutableLiveData<>();
  private final MutableLiveData<SearchWheel.SearchOption> mSearchOption = new MutableLiveData<>();
  private final MutableLiveData<Boolean> mTrackRecorderState = new MutableLiveData<>(TrackRecorder.nativeIsTrackRecordingEnabled());

  public MutableLiveData<Boolean> getButtonsHidden()
  {
    return mButtonsHidden;
  }

  public void setButtonsHidden(boolean buttonsHidden)
  {
    mButtonsHidden.setValue(buttonsHidden);
  }

  public MutableLiveData<Float> getBottomButtonsHeight()
  {
    return mBottomButtonsHeight;
  }

  public void setBottomButtonsHeight(float height)
  {
    mBottomButtonsHeight.setValue(height);
  }

  public MutableLiveData<Integer> getTopButtonsMarginTop()
  {
    return mTopButtonsMarginTop;
  }

  public void setTopButtonsMarginTop(int margin)
  {
    mTopButtonsMarginTop.setValue(margin);
  }

  public MutableLiveData<MapButtonsController.LayoutMode> getLayoutMode()
  {
    return mLayoutMode;
  }

  public void setLayoutMode(MapButtonsController.LayoutMode layoutMode)
  {
    mLayoutMode.setValue(layoutMode);
  }

  public MutableLiveData<Integer> getMyPositionMode()
  {
    return mMyPositionMode;
  }

  public void setMyPositionMode(int mode)
  {
    mMyPositionMode.setValue(mode);
  }

  public MutableLiveData<SearchWheel.SearchOption> getSearchOption()
  {
    return mSearchOption;
  }

  public void setSearchOption(@Nullable SearchWheel.SearchOption searchOption)
  {
    mSearchOption.setValue(searchOption);
  }

  public void setTrackRecorderState(boolean state)
  {
    mTrackRecorderState.setValue(state);
  }

  public MutableLiveData<Boolean> getTrackRecorderState()
  {
    return mTrackRecorderState;
  }
}
