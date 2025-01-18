package app.organicmaps.editor;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.CallSuper;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.core.view.ViewCompat;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import app.organicmaps.MwmApplication;
import app.organicmaps.R;
import app.organicmaps.base.BaseMwmToolbarFragment;
import app.organicmaps.bookmarks.data.Metadata;
import app.organicmaps.editor.data.Language;
import app.organicmaps.editor.data.LocalizedName;
import app.organicmaps.editor.data.LocalizedStreet;
import app.organicmaps.editor.data.NamesDataSource;
import app.organicmaps.editor.data.PhoneFragment;
import app.organicmaps.util.ConnectionState;
import app.organicmaps.util.UiUtils;
import app.organicmaps.util.Utils;
import app.organicmaps.util.WindowInsetUtils.PaddingInsetsListener;
import app.organicmaps.widget.SearchToolbarController;
import app.organicmaps.widget.ToolbarController;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;

import java.util.ArrayList;
import java.util.List;

public class EditorHostFragment extends BaseMwmToolbarFragment implements View.OnClickListener,
    LanguagesFragment.Listener
{
  private boolean mIsNewObject;
  @Nullable
  private View mToolbarInnerLayout;
  @Nullable
  private View mSave;

  enum Mode
  {
    MAP_OBJECT,
    OPENING_HOURS,
    STREET,
    CUISINE,
    LANGUAGE,
    PHONE,
    SELF_SERVICE
  }

  private Mode mMode;

  /**
   * A list of localized names added by a user and those that were in metadata.
   */
  private static final List<LocalizedName> sNames = new ArrayList<>();
  /**
   * Count of names which should always be shown.
   */
  private int mMandatoryNamesCount = 0;

  private static final String NOOB_ALERT_SHOWN = "Alert_for_noob_was_shown";

  /**
   *   Used in MultilanguageAdapter to show, select and remove items.
   */
  List<LocalizedName> getNames()
  {
    return sNames;
  }

  public LocalizedName[] getNamesAsArray()
  {
    return sNames.toArray(new LocalizedName[sNames.size()]);
  }

  void setNames(LocalizedName[] names)
  {
    sNames.clear();
    for (LocalizedName name : names)
    {
      addName(name);
    }
  }

  void addName(LocalizedName name)
  {
    sNames.add(name);
  }

  public int getMandatoryNamesCount()
  {
    return mMandatoryNamesCount;
  }

  public void setMandatoryNamesCount(int mandatoryNamesCount)
  {
    mMandatoryNamesCount = mandatoryNamesCount;
  }

  private void fillNames()
  {
    NamesDataSource namesDataSource = Editor.nativeGetNamesDataSource();
    setNames(namesDataSource.getNames());
    setMandatoryNamesCount(namesDataSource.getMandatoryNamesCount());
    editMapObject();
  }

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState)
  {
    return inflater.inflate(R.layout.fragment_editor_host, container, false);
  }

  @CallSuper
  @Override
  public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);

    final View toolbar = getToolbarController().getToolbar();
    mToolbarInnerLayout = toolbar.findViewById(R.id.toolbar_inner_layout);
    mSave = toolbar.findViewById(R.id.save);
    mSave.setOnClickListener(this);
    UiUtils.setupHomeUpButtonAsNavigationIcon(getToolbarController().getToolbar(),
                                              v -> onBackPressed());

    if (getArguments() != null)
      mIsNewObject = getArguments().getBoolean(EditorActivity.EXTRA_NEW_OBJECT, false);
    getToolbarController().setTitle(getTitle());

    fillNames();

    View fragmentContainer = view.findViewById(R.id.fragment_container);
    ViewCompat.setOnApplyWindowInsetsListener(fragmentContainer, PaddingInsetsListener.excludeTop());
  }

  @StringRes
  private int getTitle()
  {
    return mIsNewObject ? R.string.editor_add_place_title : R.string.editor_edit_place_title;
  }

  @NonNull
  @Override
  protected ToolbarController onCreateToolbarController(@NonNull View root)
  {
    return new SearchToolbarController(root, requireActivity())
    {
      @Override
      protected void onTextChanged(String query)
      {
        Fragment fragment = getChildFragmentManager().findFragmentByTag(CuisineFragment.class.getName());
        if (fragment != null)
          ((CuisineFragment) fragment).setFilter(query);
      }

      @Override
      protected boolean showBackButton()
      {
        return false;
      }
    };
  }

  @Override
  public boolean onBackPressed()
  {
    switch (mMode)
    {
      case OPENING_HOURS, STREET, CUISINE, LANGUAGE, PHONE, SELF_SERVICE -> editMapObject();
      default -> Utils.navigateToParent(requireActivity());
    }
    return true;
  }

  protected void editMapObject()
  {
    editMapObject(false /* focusToLastName */);
  }

  protected void editMapObject(boolean focusToLastName)
  {
    mMode = Mode.MAP_OBJECT;
    showSearchControls(false);
    getToolbarController().setTitle(getTitle());
    UiUtils.show(getToolbarController().getToolbar().findViewById(R.id.save));
    Bundle args = new Bundle();
    if (focusToLastName)
      args.putInt(EditorFragment.LAST_INDEX_OF_NAMES_ARRAY, sNames.size() - 1);
    FragmentManager fragmentManager = getChildFragmentManager();
    final Fragment editorFragment = fragmentManager.getFragmentFactory()
      .instantiate(requireActivity().getClassLoader(), EditorFragment.class.getName());
    editorFragment.setArguments(args);
    fragmentManager.beginTransaction()
                        .replace(R.id.fragment_container, editorFragment, EditorFragment.class.getName())
                        .commit();
  }

  protected void editTimetable()
  {
    final Bundle args = new Bundle();
    args.putString(TimetableContainerFragment.EXTRA_TIME, Editor.nativeGetOpeningHours());
    editWithFragment(Mode.OPENING_HOURS, R.string.editor_time_title, args, TimetableContainerFragment.class, false);
  }

  protected void editPhone()
  {
    final Bundle args = new Bundle();
    args.putString(PhoneFragment.EXTRA_PHONE_LIST, Editor.nativeGetPhone());
    editWithFragment(Mode.PHONE, R.string.phone_number, args, PhoneFragment.class, false);
  }

  protected void editStreet()
  {
    editWithFragment(Mode.STREET, R.string.choose_street, null, StreetFragment.class, false);
  }

  protected void editCuisine()
  {
    editWithFragment(Mode.CUISINE, R.string.select_cuisine, null, CuisineFragment.class, true);
  }

  protected void editSelfService()
  {
    editWithFragment(Mode.SELF_SERVICE, R.string.select_option, null, SelfServiceFragment.class, false);
  }

  protected void addLanguage()
  {
    Bundle args = new Bundle();
    ArrayList<String> languages = new ArrayList<>(sNames.size());
    for (LocalizedName name : sNames)
      languages.add(name.lang);
    args.putStringArrayList(LanguagesFragment.EXISTING_LOCALIZED_NAMES, languages);
    editWithFragment(Mode.LANGUAGE, R.string.choose_language, args, LanguagesFragment.class, false);
  }

  private void editWithFragment(Mode newMode, @StringRes int toolbarTitle, @Nullable Bundle args, Class<? extends Fragment> fragmentClass, boolean showSearch)
  {
    if (!setEdits())
      return;

    mMode = newMode;
    getToolbarController().setTitle(toolbarTitle);
    showSearchControls(showSearch);
    FragmentManager fragmentManager = requireActivity().getSupportFragmentManager();
    final Fragment fragment = fragmentManager.getFragmentFactory().instantiate(requireActivity().getClassLoader(), fragmentClass.getName());
    fragment.setArguments(args);
    getChildFragmentManager().beginTransaction()
                             .replace(R.id.fragment_container, fragment, fragmentClass.getName())
                             .commit();
  }

  private void showSearchControls(boolean showSearch)
  {
    ((SearchToolbarController) getToolbarController()).showSearchControls(showSearch);
    if (mToolbarInnerLayout != null && mSave != null)
    {
      // Make room for the toolbar title if the search controls are hidden.
      mToolbarInnerLayout.getLayoutParams().width = showSearch
              ? ViewGroup.LayoutParams.MATCH_PARENT
              : mSave.getLayoutParams().width;
    }
  }

  private boolean setEdits()
  {
    return ((EditorFragment) getChildFragmentManager().findFragmentByTag(EditorFragment.class.getName())).setEdits();
  }

  @Override
  public void onClick(View v)
  {
    if (v.getId() == R.id.save)
    {
      switch (mMode)
      {
        case OPENING_HOURS ->
        {
          final String timetables = ((TimetableContainerFragment) getChildFragmentManager().findFragmentByTag(TimetableContainerFragment.class.getName())).getTimetable();
          Editor.nativeSetOpeningHours(timetables);
          editMapObject();
        }
        case STREET ->
            setStreet(((StreetFragment) getChildFragmentManager().findFragmentByTag(StreetFragment.class.getName())).getStreet());
        case CUISINE ->
        {
          String[] cuisines = ((CuisineFragment) getChildFragmentManager().findFragmentByTag(CuisineFragment.class.getName())).getCuisines();
          Editor.nativeSetSelectedCuisines(cuisines);
          editMapObject();
        }
        case SELF_SERVICE ->
                setSelection(Metadata.MetadataType.FMD_SELF_SERVICE, ((SelfServiceFragment) getChildFragmentManager().findFragmentByTag(SelfServiceFragment.class.getName())).getSelection());
        case LANGUAGE -> editMapObject();
        case MAP_OBJECT ->
        {
          if (!setEdits())
            return;

          // Save object edits
          if (!MwmApplication.prefs(requireContext()).contains(NOOB_ALERT_SHOWN))
          {
            showNoobDialog();
          }
          else
          {
            saveNote();
            saveMapObjectEdits();
          }
        }
        case PHONE ->
        {
          final String phone = ((PhoneFragment) getChildFragmentManager().findFragmentByTag(PhoneFragment.class.getName())).getPhone();
          if (Editor.nativeIsPhoneValid(phone))
          {
            Editor.nativeSetPhone(phone);
            editMapObject();
          }
        }
      }
    }
  }

  private void saveMapObjectEdits()
  {
    if (Editor.nativeSaveEditedFeature())
      processEditedFeatures();
    else
      processNoFeatures();
  }

  private void processNoFeatures()
  {
    new MaterialAlertDialogBuilder(requireActivity(), R.style.MwmTheme_AlertDialog)
        .setTitle(R.string.downloader_no_space_title)
        .setPositiveButton(R.string.ok, null)
        .show();
  }

  private void processEditedFeatures()
  {
    Context context = requireContext();
    if (OsmOAuth.isAuthorized(context))
    {
      Utils.navigateToParent(requireActivity());
      return;
    }

    showLoginDialog();
  }

  private void showLoginDialog()
  {
    startActivity(new Intent(requireContext(), OsmLoginActivity.class));
    requireActivity().finish();
  }

  private void saveNote()
  {
    String tag = EditorFragment.class.getName();
    EditorFragment fragment = (EditorFragment) getChildFragmentManager().findFragmentByTag(tag);
    String note = fragment.getDescription();
    if (!TextUtils.isEmpty(note))
      Editor.nativeCreateNote(note);
  }

  private void showNoobDialog()
  {
    new MaterialAlertDialogBuilder(requireActivity(), R.style.MwmTheme_AlertDialog)
        .setTitle(R.string.editor_share_to_all_dialog_title)
        .setMessage(getString(R.string.editor_share_to_all_dialog_message_1)
            + " " + getString(R.string.editor_share_to_all_dialog_message_2))
        .setPositiveButton(android.R.string.ok, (dlg, which) -> {
          MwmApplication.prefs(requireContext()).edit()
              .putBoolean(NOOB_ALERT_SHOWN, true)
              .apply();
          saveNote();
          saveMapObjectEdits();
        })
        .setNegativeButton(android.R.string.cancel, null)
        .show();
  }

  public void setStreet(LocalizedStreet street)
  {
    Editor.nativeSetStreet(street);
    editMapObject();
  }

  public void setSelection(Metadata.MetadataType metadata, String selection)
  {
    Editor.nativeSetMetadata(metadata.toInt(), selection);
    editMapObject();
  }

  public boolean addingNewObject()
  {
    return mIsNewObject;
  }

  @Override
  public void onLanguageSelected(Language lang)
  {
    String name = "";
    addName(Editor.nativeMakeLocalizedName(lang.code, name));
    editMapObject(true /* focusToLastName */);
  }
}
