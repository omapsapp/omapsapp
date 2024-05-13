package app.organicmaps.editor;

import android.annotation.SuppressLint;
import android.app.Dialog;
import android.content.Context;
import android.content.res.ColorStateList;
import android.os.Bundle;
import android.text.format.DateFormat;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import android.widget.TimePicker;

import androidx.annotation.IntRange;
import androidx.annotation.NonNull;
import androidx.annotation.StyleRes;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.fragment.app.FragmentManager;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.android.material.tabs.TabLayout;
import app.organicmaps.R;
import app.organicmaps.base.BaseMwmDialogFragment;
import app.organicmaps.sdk.editor.data.HoursMinutes;
import app.organicmaps.sdk.util.DateUtils;
import app.organicmaps.util.ThemeUtils;
import app.organicmaps.util.Utils;

public class HoursMinutesPickerFragment extends BaseMwmDialogFragment
{
  private static final String EXTRA_FROM = "HoursMinutesFrom";
  private static final String EXTRA_TO = "HoursMinutesTo";
  private static final String EXTRA_SELECT_FIRST = "SelectedTab";
  private static final String EXTRA_ID = "Id";

  public static final int TAB_FROM = 0;
  public static final int TAB_TO = 1;

  private HoursMinutes mFrom;
  private HoursMinutes mTo;

  private TimePicker mPicker;
  private View mPickerHoursLabel;

  @IntRange(from = 0, to = 1) private int mSelectedTab;
  private TabLayout mTabs;

  private int mId;
  private Button mOkButton;

  public interface OnPickListener
  {
    void onHoursMinutesPicked(HoursMinutes from, HoursMinutes to, int id);
  }

  public static void pick(Context context, FragmentManager manager, @NonNull HoursMinutes from, @NonNull HoursMinutes to,
                          @IntRange(from = 0, to = 1) int selectedPosition, int id)
  {
    final Bundle args = new Bundle();
    args.putParcelable(EXTRA_FROM, from);
    args.putParcelable(EXTRA_TO, to);
    args.putInt(EXTRA_SELECT_FIRST, selectedPosition);
    args.putInt(EXTRA_ID, id);

    final HoursMinutesPickerFragment fragment = (HoursMinutesPickerFragment) manager.getFragmentFactory()
      .instantiate(context.getClassLoader(), HoursMinutesPickerFragment.class.getName());
    fragment.setArguments(args);
    fragment.show(manager, null);
  }

  @NonNull
  @Override
  public Dialog onCreateDialog(Bundle savedInstanceState)
  {
    readArgs();
    final View root = createView();
    //noinspection ConstantConditions
    mTabs.getTabAt(mSelectedTab).select();

    @StyleRes final int theme = ThemeUtils.isNightTheme(requireContext()) ?
        R.style.MwmMain_DialogFragment_TimePicker_Night :
        R.style.MwmMain_DialogFragment_TimePicker;
    final AlertDialog dialog = new MaterialAlertDialogBuilder(requireActivity(), theme)
        .setView(root)
        .setNegativeButton(R.string.cancel, null)
        .setPositiveButton(R.string.ok, null)
        .setCancelable(true)
        .create();

    dialog.setOnShowListener(dialogInterface -> {
      mOkButton = dialog.getButton(AlertDialog.BUTTON_POSITIVE);
      mOkButton.setOnClickListener(v -> {
        if (mSelectedTab == TAB_FROM)
        {
          //noinspection ConstantConditions
          mTabs.getTabAt(TAB_TO).select();
          return;
        }

        saveHoursMinutes();
        dismiss();
        if (getParentFragment() instanceof OnPickListener)
          ((OnPickListener) getParentFragment()).onHoursMinutesPicked(mFrom, mTo, mId);
      });
      refreshPicker();
    });

    return dialog;
  }

  private void readArgs()
  {
    final Bundle args = getArguments();
    if (args == null)
      throw new IllegalArgumentException("Args must not be null");
    mFrom = Utils.getParcelable(args, EXTRA_FROM, HoursMinutes.class);
    mTo = Utils.getParcelable(args, EXTRA_TO, HoursMinutes.class);
    mSelectedTab = args.getInt(EXTRA_SELECT_FIRST);
    mId = args.getInt(EXTRA_ID);
  }

  private View createView()
  {
    final LayoutInflater inflater = LayoutInflater.from(requireActivity());
    @SuppressLint("InflateParams")
    final View root = inflater.inflate(R.layout.fragment_timetable_picker, null);

    mPicker = root.findViewById(R.id.picker);
    mPicker.setIs24HourView(DateFormat.is24HourFormat(requireActivity()));
    @SuppressLint("DiscouragedApi")
    int id = getResources().getIdentifier("hours", "id", "android");
    if (id != 0)
    {
      mPickerHoursLabel = mPicker.findViewById(id);
      if (!(mPickerHoursLabel instanceof TextView))
        mPickerHoursLabel = null;
    }

    mTabs = root.findViewById(R.id.tabs);
    TextView tabView = (TextView) inflater.inflate(R.layout.tab_timepicker, mTabs, false);
    tabView.setText(getResources().getString(R.string.editor_time_from));
    final ColorStateList textColor = AppCompatResources.getColorStateList(requireContext(),
        ThemeUtils.isNightTheme(requireContext()) ? R.color.accent_color_selector_night
                                                  : R.color.accent_color_selector);
    tabView.setTextColor(textColor);
    mTabs.addTab(mTabs.newTab().setCustomView(tabView), true);
    tabView = (TextView) inflater.inflate(R.layout.tab_timepicker, mTabs, false);
    tabView.setText(getResources().getString(R.string.editor_time_to));
    tabView.setTextColor(textColor);
    mTabs.addTab(mTabs.newTab().setCustomView(tabView), true);
    mTabs.setOnTabSelectedListener(new TabLayout.OnTabSelectedListener()
    {
      @Override
      public void onTabSelected(TabLayout.Tab tab)
      {
        if (!isInit())
          return;

        saveHoursMinutes();
        mSelectedTab = tab.getPosition();
        refreshPicker();
        if (mPickerHoursLabel != null)
          mPickerHoursLabel.performClick();
      }

      @Override
      public void onTabUnselected(TabLayout.Tab tab) {}

      @Override
      public void onTabReselected(TabLayout.Tab tab) {}
    });

    return root;
  }

  private void saveHoursMinutes()
  {
    boolean is24HourFormat = DateUtils.is24HourFormat(requireContext());
    final HoursMinutes hoursMinutes = new HoursMinutes(mPicker.getCurrentHour(),
                                                       mPicker.getCurrentMinute(), is24HourFormat);
    if (mSelectedTab == TAB_FROM)
      mFrom = hoursMinutes;
    else
      mTo = hoursMinutes;
  }

  private boolean isInit()
  {
    return mOkButton != null && mPicker != null;
  }

  private void refreshPicker()
  {
    if (!isInit())
      return;

    HoursMinutes hoursMinutes;
    int okBtnRes;
    if (mSelectedTab == TAB_FROM)
    {
      hoursMinutes = mFrom;
      okBtnRes = R.string.next_button;
    }
    else
    {
      hoursMinutes = mTo;
      okBtnRes = R.string.ok;
    }
    mPicker.setCurrentMinute((int) hoursMinutes.minutes);
    mPicker.setCurrentHour((int) hoursMinutes.hours);
    mOkButton.setText(okBtnRes);
  }
}
