package app.organicmaps.widget.placepage;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.AttrRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.Observer;
import androidx.lifecycle.ViewModelProvider;
import app.organicmaps.R;
import app.organicmaps.util.Graphics;
import app.organicmaps.util.WindowInsetUtils.PaddingInsetsListener;
import app.organicmaps.util.bottomsheet.MenuBottomSheetFragment;

import java.util.ArrayList;
import java.util.List;

public final class PlacePageButtons extends Fragment implements Observer<List<PlacePageButtons.ButtonType>>
{
  public static final String PLACEPAGE_MORE_MENU_ID = "PLACEPAGE_MORE_MENU_BOTTOM_SHEET";
  private int mMaxButtons;

  private PlacePageButtonClickListener mItemListener;
  private ViewGroup mButtonsContainer;
  private PlacePageViewModel mViewModel;

  @Nullable
  @Override
  public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState)
  {
    mViewModel = new ViewModelProvider(requireActivity()).get(PlacePageViewModel.class);
    return inflater.inflate(R.layout.pp_buttons_fragment, container, false);
  }

  @Override
  public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);
    mButtonsContainer = view.findViewById(R.id.container);
    final PaddingInsetsListener insetsListener = new PaddingInsetsListener.Builder()
        .setInsetsTypeMask(WindowInsetsCompat.Type.systemBars() | WindowInsetsCompat.Type.displayCutout())
        .setExcludeTop()
        .build();
    ViewCompat.setOnApplyWindowInsetsListener(view, insetsListener);
    mMaxButtons = getResources().getInteger(R.integer.pp_buttons_max);

    Fragment parentFragment = getParentFragment();
    mItemListener = (PlacePageButtonClickListener) parentFragment;

    createButtons(mViewModel.getCurrentButtons().getValue());
  }

  @Override
  public void onStart()
  {
    super.onStart();
    mViewModel.getCurrentButtons().observe(requireActivity(), this);
  }

  @Override
  public void onStop()
  {
    super.onStop();
    mViewModel.getCurrentButtons().removeObserver(this);
  }

  private @NonNull
  List<PlacePageButton> collectButtons(List<PlacePageButtons.ButtonType> items)
  {
    List<PlacePageButton> res = new ArrayList<>();
    int count = items.size();
    if (items.size() > mMaxButtons)
      count = mMaxButtons - 1;

    for (int i = 0; i < count; i++)
      res.add(PlacePageButtonFactory.createButton(items.get(i), requireContext()));

    if (items.size() > mMaxButtons)
      res.add(PlacePageButtonFactory.createButton(ButtonType.MORE, requireContext()));
    return res;
  }

  private void showMoreBottomSheet()
  {
    MenuBottomSheetFragment.newInstance(PLACEPAGE_MORE_MENU_ID)
                           .show(getParentFragmentManager(), PLACEPAGE_MORE_MENU_ID);
  }

  private void createButtons(@Nullable List<ButtonType> buttons)
  {
    if (buttons == null)
      return;
    List<PlacePageButton> shownButtons = collectButtons(buttons);
    mButtonsContainer.removeAllViews();
    for (PlacePageButton button : shownButtons)
      mButtonsContainer.addView(createButton(button));
  }

  private View createButton(@NonNull final PlacePageButton current)
  {
    LayoutInflater inflater = LayoutInflater.from(requireContext());
    View parent = inflater.inflate(R.layout.place_page_button, mButtonsContainer, false);

    ImageView icon = parent.findViewById(R.id.icon);
    TextView title = parent.findViewById(R.id.title);

    title.setText(current.getTitle());
    @AttrRes final int tint = current.getType() == ButtonType.BOOKMARK_DELETE
                              ? R.attr.iconTintActive
                              : R.attr.iconTint;
    icon.setImageDrawable(Graphics.tint(getContext(), current.getIcon(), tint));
    parent.setOnClickListener((view) -> {
      if (current.getType() == ButtonType.MORE)
        showMoreBottomSheet();
      else
        mItemListener.onPlacePageButtonClick(current.getType());
    });
    return parent;
  }

  @Override
  public void onChanged(List<ButtonType> buttonTypes)
  {
    createButtons(buttonTypes);
  }

  public enum ButtonType
  {
    BACK,
    BOOKMARK_SAVE,
    BOOKMARK_DELETE,
    ROUTE_FROM,
    ROUTE_TO,
    ROUTE_ADD,
    ROUTE_REMOVE,
    ROUTE_AVOID_TOLL,
    ROUTE_AVOID_FERRY,
    ROUTE_AVOID_UNPAVED,
    MORE
  }

  public interface PlacePageButtonClickListener
  {
    void onPlacePageButtonClick(ButtonType item);
  }
}
