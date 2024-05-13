package app.organicmaps.editor;

import android.content.Context;
import android.os.Bundle;
import android.text.InputType;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.CallSuper;
import androidx.annotation.DrawableRes;
import androidx.annotation.IdRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.appcompat.widget.SwitchCompat;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.android.material.textfield.TextInputLayout;
import com.google.android.material.textfield.TextInputEditText;
import app.organicmaps.sdk.Framework;
import app.organicmaps.R;
import app.organicmaps.base.BaseMwmFragment;
import app.organicmaps.sdk.bookmarks.data.Metadata;
import app.organicmaps.dialog.EditTextDialogFragment;
import app.organicmaps.sdk.editor.Editor;
import app.organicmaps.sdk.editor.OpeningHours;
import app.organicmaps.sdk.editor.data.LocalizedName;
import app.organicmaps.sdk.editor.data.LocalizedStreet;
import app.organicmaps.editor.data.TimeFormatUtils;
import app.organicmaps.sdk.editor.data.Timetable;
import app.organicmaps.util.Graphics;
import app.organicmaps.util.InputUtils;
import app.organicmaps.util.Option;
import app.organicmaps.sdk.util.StringUtils;
import app.organicmaps.util.UiUtils;
import app.organicmaps.util.Utils;

import java.util.HashMap;
import java.util.Map;

public class EditorFragment extends BaseMwmFragment implements View.OnClickListener
{
  final static String LAST_INDEX_OF_NAMES_ARRAY = "LastIndexOfNamesArray";

  private TextView mCategory;
  private View mCardName;
  private View mCardAddress;
  private View mCardDetails;

  private RecyclerView mNamesView;

  private final RecyclerView.AdapterDataObserver mNamesObserver = new RecyclerView.AdapterDataObserver()
  {
    @Override
    public void onChanged()
    {
      refreshNamesCaption();
    }

    @Override
    public void onItemRangeChanged(int positionStart, int itemCount)
    {
      refreshNamesCaption();
    }

    @Override
    public void onItemRangeInserted(int positionStart, int itemCount)
    {
      refreshNamesCaption();
    }

    @Override
    public void onItemRangeRemoved(int positionStart, int itemCount)
    {
      refreshNamesCaption();
    }

    @Override
    public void onItemRangeMoved(int fromPosition, int toPosition, int itemCount)
    {
      refreshNamesCaption();
    }
  };

  private MultilanguageAdapter mNamesAdapter;
  private TextView mNamesCaption;
  private TextView mAddLanguage;
  private TextView mMoreLanguages;

  private TextView mStreet;
  private TextInputEditText mHouseNumber;
  private View mBlockLevels;
  private TextInputEditText mBuildingLevels;

  // Define Metadata entries, that have more tricky logic, separately.
  private TextView mPhone;
  private TextView mEditPhoneLink;
  private TextView mCuisine;
  private SwitchCompat mWifi;

  // Default Metadata entries.
  private final class MetadataEntry
  {
    TextInputEditText mEdit;
    TextInputLayout mInput;
  }
  Map<Metadata.MetadataType, MetadataEntry> mMetadata = new HashMap<>();

  private void initMetadataEntry(Metadata.MetadataType type, @StringRes int error)
  {
    final MetadataEntry e = mMetadata.get(type);
    final int id = type.toInt();
    e.mEdit.setText(Editor.nativeGetMetadata(id));
    e.mEdit.addTextChangedListener(new StringUtils.SimpleTextWatcher()
    {
      @Override
      public void onTextChanged(CharSequence s, int start, int before, int count)
      {
        UiUtils.setInputError(e.mInput, Editor.nativeIsMetadataValid(id, s.toString()) ? 0 : error);
      }
    });
  }

  private TextInputLayout mInputHouseNumber;
  private TextInputLayout mInputBuildingLevels;

  private View mEmptyOpeningHours;
  private TextView mOpeningHours;
  private View mEditOpeningHours;
  private TextInputEditText mDescription;
  private final Map<Metadata.MetadataType, View> mDetailsBlocks = new HashMap<>();
  private TextView mReset;

  private EditorHostFragment mParent;

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState)
  {
    return inflater.inflate(R.layout.fragment_editor, container, false);
  }

  @CallSuper
  @Override
  public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
  {
    mParent = (EditorHostFragment) getParentFragment();

    initViews(view);

    mCategory.setText(Utils.getLocalizedFeatureType(requireContext(), Editor.nativeGetCategory()));
    final LocalizedStreet street = Editor.nativeGetStreet();
    mStreet.setText(street.defaultName);

    mHouseNumber.setText(Editor.nativeGetHouseNumber());
    mHouseNumber.addTextChangedListener(new StringUtils.SimpleTextWatcher()
    {
      @Override
      public void onTextChanged(CharSequence s, int start, int before, int count)
      {
        UiUtils.setInputError(mInputHouseNumber, Editor.nativeIsHouseValid(s.toString()) ? 0 : R.string.error_enter_correct_house_number);
      }
    });

    initMetadataEntry(Metadata.MetadataType.FMD_POSTCODE, R.string.error_enter_correct_zip_code);

    mBuildingLevels.setText(Editor.nativeGetBuildingLevels());
    mBuildingLevels.addTextChangedListener(new StringUtils.SimpleTextWatcher()
    {
      @Override
      public void onTextChanged(CharSequence s, int start, int before, int count)
      {
        final Context context = mInputBuildingLevels.getContext();
        final boolean isValid = Editor.nativeIsLevelValid(s.toString());
        UiUtils.setInputError(mInputBuildingLevels, isValid ? null : context.getString(R.string.error_enter_correct_storey_number, Editor.nativeGetMaxEditableBuildingLevels()));
      }
    });

    mPhone.setText(Editor.nativeGetPhone());

    initMetadataEntry(Metadata.MetadataType.FMD_WEBSITE, R.string.error_enter_correct_web);
    initMetadataEntry(Metadata.MetadataType.FMD_EMAIL, R.string.error_enter_correct_email);
    initMetadataEntry(Metadata.MetadataType.FMD_CONTACT_FACEBOOK, R.string.error_enter_correct_facebook_page);
    initMetadataEntry(Metadata.MetadataType.FMD_CONTACT_INSTAGRAM, R.string.error_enter_correct_instagram_page);
    initMetadataEntry(Metadata.MetadataType.FMD_CONTACT_TWITTER, R.string.error_enter_correct_twitter_page);
    initMetadataEntry(Metadata.MetadataType.FMD_CONTACT_VK, R.string.error_enter_correct_vk_page);
    initMetadataEntry(Metadata.MetadataType.FMD_CONTACT_LINE, R.string.error_enter_correct_line_page);

    mCuisine.setText(Editor.nativeGetFormattedCuisine());
    initMetadataEntry(Metadata.MetadataType.FMD_OPERATOR, 0);
    mWifi.setChecked(Editor.nativeHasWifi());
    refreshOpeningTime();
    refreshEditableFields();
    refreshResetButton();
  }

  @Override
  public void onSaveInstanceState(Bundle outState)
  {
    super.onSaveInstanceState(outState);
    setEdits();
  }

  boolean setEdits()
  {
    if (!validateFields())
      return false;

    Editor.nativeSetHouseNumber(mHouseNumber.getText().toString());
    Editor.nativeSetBuildingLevels(mBuildingLevels.getText().toString());
    Editor.nativeSetHasWifi(mWifi.isChecked());
    Editor.nativeSetNames(mParent.getNamesAsArray());

    for (var e : mMetadata.entrySet())
      Editor.nativeSetMetadata(e.getKey().toInt(), e.getValue().mEdit.getText().toString());

    return true;
  }

  @NonNull
  protected String getDescription()
  {
    return mDescription.getText().toString().trim();
  }

  private boolean validateFields()
  {
    if (Editor.nativeIsAddressEditable())
    {
      if (!Editor.nativeIsHouseValid(mHouseNumber.getText().toString()))
      {
        mHouseNumber.requestFocus();
        InputUtils.showKeyboard(mHouseNumber);
        return false;
      }

      if (!Editor.nativeIsLevelValid(mBuildingLevels.getText().toString()))
      {
        mBuildingLevels.requestFocus();
        InputUtils.showKeyboard(mBuildingLevels);
        return false;
      }
    }

    for (var e : mMetadata.entrySet())
    {
      final TextInputEditText edit = e.getValue().mEdit;
      if (!Editor.nativeIsMetadataValid(e.getKey().toInt(), edit.getText().toString()))
      {
        edit.requestFocus();
        InputUtils.showKeyboard(edit);
        return false;
      }
    }

    if (!Editor.nativeIsPhoneValid(mPhone.getText().toString()))
    {
      mPhone.requestFocus();
      InputUtils.showKeyboard(mPhone);
      return false;
    }

    return validateNames();
  }

  private boolean validateNames()
  {
    for (int pos = 0; pos < mNamesAdapter.getItemCount(); pos++)
    {
      LocalizedName localizedName = mNamesAdapter.getNameAtPos(pos);
      if (Editor.nativeIsNameValid(localizedName.name))
        continue;

      View nameView = mNamesView.getChildAt(pos);
      nameView.requestFocus();

      InputUtils.showKeyboard(nameView);

      return false;
    }

    return true;
  }

  private void refreshEditableFields()
  {
    UiUtils.showIf(Editor.nativeIsNameEditable(), mCardName);
    UiUtils.showIf(Editor.nativeIsAddressEditable(), mCardAddress);
    UiUtils.showIf(Editor.nativeIsBuilding() && !Editor.nativeIsPointType(), mBlockLevels);

    final int[] editableDetails = Editor.nativeGetEditableProperties();
    if (editableDetails.length == 0)
    {
      UiUtils.hide(mCardDetails);
      return;
    }

    for (var e : mDetailsBlocks.entrySet())
      UiUtils.hide(e.getValue());

    boolean anyEditableDetails = false;
    for (int type : editableDetails)
    {
      final View detailsBlock = mDetailsBlocks.get(Metadata.MetadataType.fromInt(type));
      if (detailsBlock == null)
        continue;

      anyEditableDetails = true;
      UiUtils.show(detailsBlock);
    }
    UiUtils.showIf(anyEditableDetails, mCardDetails);
  }

  private void refreshOpeningTime()
  {
    final String openingHours = Editor.nativeGetOpeningHours();
    if (TextUtils.isEmpty(openingHours) || !OpeningHours.nativeIsTimetableStringValid(openingHours))
    {
      UiUtils.show(mEmptyOpeningHours);
      UiUtils.hide(mOpeningHours, mEditOpeningHours);
    }
    else
    {
      final Timetable[] timetables = OpeningHours.nativeTimetablesFromString(openingHours);
      String content = timetables == null ? openingHours
                                          : TimeFormatUtils.formatTimetables(getResources(),
                                                                             openingHours,
                                                                             timetables);
      UiUtils.hide(mEmptyOpeningHours);
      UiUtils.setTextAndShow(mOpeningHours, content);
      UiUtils.show(mEditOpeningHours);
    }
  }

  private void initNamesView(final View view)
  {
    mNamesCaption = view.findViewById(R.id.show_additional_names);
    mNamesCaption.setOnClickListener(this);

    mAddLanguage = view.findViewById(R.id.add_langs);
    mAddLanguage.setOnClickListener(this);

    mMoreLanguages = view.findViewById(R.id.more_names);
    mMoreLanguages.setOnClickListener(this);

    mNamesView = view.findViewById(R.id.recycler);
    mNamesView.setNestedScrollingEnabled(false);
    mNamesView.setLayoutManager(new LinearLayoutManager(requireActivity()));
    mNamesAdapter = new MultilanguageAdapter(mParent);
    mNamesView.setAdapter(mNamesAdapter);
    mNamesAdapter.registerAdapterDataObserver(mNamesObserver);

    final Bundle args = getArguments();
    if (args == null || !args.containsKey(LAST_INDEX_OF_NAMES_ARRAY))
    {
      showAdditionalNames(false);
      return;
    }
    showAdditionalNames(true);
    UiUtils.waitLayout(mNamesView, () -> {
      LinearLayoutManager lm = (LinearLayoutManager) mNamesView.getLayoutManager();
      int position = args.getInt(LAST_INDEX_OF_NAMES_ARRAY);

      View nameItem = lm.findViewByPosition(position);

      int cvNameTop = mCardName.getTop();
      int nameItemTop = nameItem.getTop();

      view.scrollTo(0, cvNameTop + nameItemTop);

      // TODO(mgsergio): Uncomment if focus and keyboard are required.
      // TODO(mgsergio): Keyboard doesn't want to hide. Only pressing back button works.
      // View nameItemInput = nameItem.findViewById(R.id.input);
      // nameItemInput.requestFocus();
      // InputUtils.showKeyboard(nameItemInput);
    });
  }

  private View initBlock(View view, Metadata.MetadataType type, @IdRes int idBlock,
                         @DrawableRes int idIcon, @StringRes int idName, int inputType)
  {
    View block = view.findViewById(idBlock);
    MetadataEntry e = new MetadataEntry();
    e.mEdit = findInputAndInitBlock(block, idIcon, idName);
    if (inputType > 0)
      e.mEdit.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_MULTI_LINE | inputType);
    e.mInput = block.findViewById(R.id.custom_input);
    mMetadata.put(type, e);
    return block;
  }

  private void initViews(View view)
  {
    final View categoryBlock = view.findViewById(R.id.category);
    categoryBlock.setOnClickListener(this);
    // TODO show icon and fill it when core will implement that
    UiUtils.hide(categoryBlock.findViewById(R.id.icon));
    mCategory = categoryBlock.findViewById(R.id.name);
    mCardName = view.findViewById(R.id.cv__name);
    mCardAddress = view.findViewById(R.id.cv__address);
    mCardDetails = view.findViewById(R.id.cv__details);
    initNamesView(view);

    // Address
    view.findViewById(R.id.block_street).setOnClickListener(this);
    mStreet = view.findViewById(R.id.street);
    View blockHouseNumber = view.findViewById(R.id.block_building);
    mHouseNumber = findInputAndInitBlock(blockHouseNumber, 0, R.string.house_number);
    mInputHouseNumber = blockHouseNumber.findViewById(R.id.custom_input);

    initBlock(view, Metadata.MetadataType.FMD_POSTCODE, R.id.block_zipcode, 0, R.string.editor_zip_code, 0);

    // Details
    mBlockLevels = view.findViewById(R.id.block_levels);
    mBuildingLevels = findInputAndInitBlock(mBlockLevels, 0,
        getString(R.string.editor_storey_number, Editor.nativeGetMaxEditableBuildingLevels()));
    mBuildingLevels.setInputType(InputType.TYPE_CLASS_NUMBER);
    mInputBuildingLevels = mBlockLevels.findViewById(R.id.custom_input);
    View blockPhone = view.findViewById(R.id.block_phone);
    mPhone = blockPhone.findViewById(R.id.phone);
    mEditPhoneLink = blockPhone.findViewById(R.id.edit_phone);
    mEditPhoneLink.setOnClickListener(this);
    mPhone.setOnClickListener(this);
    initBlock(view, Metadata.MetadataType.FMD_WEBSITE, R.id.block_website,
            R.drawable.ic_website, R.string.website, InputType.TYPE_TEXT_VARIATION_URI);
    initBlock(view, Metadata.MetadataType.FMD_EMAIL, R.id.block_email,
            R.drawable.ic_email, R.string.email, InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS);
    initBlock(view, Metadata.MetadataType.FMD_CONTACT_FACEBOOK, R.id.block_facebook,
            R.drawable.ic_facebook_white, R.string.facebook, InputType.TYPE_TEXT_VARIATION_URI);
    initBlock(view, Metadata.MetadataType.FMD_CONTACT_INSTAGRAM, R.id.block_instagram,
            R.drawable.ic_instagram_white, R.string.instagram, InputType.TYPE_TEXT_VARIATION_URI);
    initBlock(view, Metadata.MetadataType.FMD_CONTACT_TWITTER, R.id.block_twitter,
            R.drawable.ic_twitterx_white, R.string.twitter, InputType.TYPE_TEXT_VARIATION_URI);
    initBlock(view, Metadata.MetadataType.FMD_CONTACT_VK, R.id.block_vk,
            R.drawable.ic_vk_white, R.string.vk, InputType.TYPE_TEXT_VARIATION_URI);
    initBlock(view, Metadata.MetadataType.FMD_CONTACT_LINE, R.id.block_line,
            R.drawable.ic_line_white, R.string.editor_line_social_network, InputType.TYPE_TEXT_VARIATION_URI);
    initBlock(view, Metadata.MetadataType.FMD_OPERATOR, R.id.block_operator,
            R.drawable.ic_operator, R.string.editor_operator, 0);

    View blockCuisine = view.findViewById(R.id.block_cuisine);
    blockCuisine.setOnClickListener(this);
    mCuisine = view.findViewById(R.id.cuisine);

    View blockWifi = view.findViewById(R.id.block_wifi);
    mWifi = view.findViewById(R.id.sw__wifi);
    blockWifi.setOnClickListener(this);
    View blockOpeningHours = view.findViewById(R.id.block_opening_hours);
    mEditOpeningHours = blockOpeningHours.findViewById(R.id.edit_opening_hours);
    mEditOpeningHours.setOnClickListener(this);
    mEmptyOpeningHours = blockOpeningHours.findViewById(R.id.empty_opening_hours);
    mEmptyOpeningHours.setOnClickListener(this);
    mOpeningHours = blockOpeningHours.findViewById(R.id.opening_hours);
    mOpeningHours.setOnClickListener(this);
    final View cardMore = view.findViewById(R.id.cv__more);
    mDescription = findInput(cardMore);
    cardMore.findViewById(R.id.about_osm).setOnClickListener(this);
    mReset = view.findViewById(R.id.reset);
    mReset.setOnClickListener(this);

    mDetailsBlocks.put(Metadata.MetadataType.FMD_OPEN_HOURS, blockOpeningHours);
    mDetailsBlocks.put(Metadata.MetadataType.FMD_PHONE_NUMBER, blockPhone);
    mDetailsBlocks.put(Metadata.MetadataType.FMD_CUISINE, blockCuisine);
    mDetailsBlocks.put(Metadata.MetadataType.FMD_INTERNET, blockWifi);
  }

  private static TextInputEditText findInput(View blockWithInput)
  {
    return blockWithInput.findViewById(R.id.input);
  }

  private TextInputEditText findInputAndInitBlock(View blockWithInput, @DrawableRes int icon, @StringRes int hint)
  {
    return findInputAndInitBlock(blockWithInput, icon, getString(hint));
  }

  private static TextInputEditText findInputAndInitBlock(View blockWithInput, @DrawableRes int icon, String hint)
  {
    ((ImageView) blockWithInput.findViewById(R.id.icon)).setImageResource(icon);
    final TextInputLayout input = blockWithInput.findViewById(R.id.custom_input);
    input.setHint(hint);
    return input.findViewById(R.id.input);
  }

  @Override
  public void onClick(View v)
  {
    final int id = v.getId();
    if (id == R.id.edit_opening_hours || id == R.id.empty_opening_hours || id == R.id.opening_hours)
      mParent.editTimetable();
    else if (id == R.id.phone || id == R.id.edit_phone)
      mParent.editPhone();
    else if (id == R.id.block_wifi)
      mWifi.toggle();
    else if (id == R.id.block_street)
      mParent.editStreet();
    else if (id == R.id.block_cuisine)
      mParent.editCuisine();
    else if (id == R.id.category)
      mParent.editCategory();
    else if (id == R.id.more_names || id == R.id.show_additional_names)
    {
      if (!mNamesAdapter.areAdditionalLanguagesShown() || validateNames())
        showAdditionalNames(!mNamesAdapter.areAdditionalLanguagesShown());
    }
    else if (id == R.id.add_langs)
      mParent.addLanguage();
    else if (id == R.id.about_osm)
      Utils.openUrl(requireActivity(), getString(R.string.osm_wiki_about_url));
    else if (id == R.id.reset)
      reset();
  }

  private void showAdditionalNames(boolean show)
  {
    mNamesAdapter.showAdditionalLanguages(show);

    refreshNamesCaption();
  }

  private void refreshNamesCaption()
  {
    if (mNamesAdapter.getNamesCount() <= mNamesAdapter.getMandatoryNamesCount())
      setNamesArrow(0 /* arrowResourceId */);  // bind arrow with empty resource (do not draw arrow)
    else if (mNamesAdapter.areAdditionalLanguagesShown())
      setNamesArrow(R.drawable.ic_expand_less);
    else
      setNamesArrow(R.drawable.ic_expand_more);

    boolean showAddLanguage = mNamesAdapter.getNamesCount() <= mNamesAdapter.getMandatoryNamesCount() ||
      mNamesAdapter.areAdditionalLanguagesShown();

    UiUtils.showIf(showAddLanguage, mAddLanguage);
    UiUtils.showIf(!showAddLanguage, mMoreLanguages);
  }

  // Bind arrow in the top right corner of names caption with needed resource.
  private void setNamesArrow(@DrawableRes int arrowResourceId)
  {
    if (arrowResourceId == 0)
    {
      mNamesCaption.setCompoundDrawablesRelativeWithIntrinsicBounds(null, null, null, null);
      return;
    }

    mNamesCaption.setCompoundDrawablesRelativeWithIntrinsicBounds(
      null,
      null,
      Graphics.tint(requireActivity(), arrowResourceId, R.attr.iconTint),
      null);
  }

  private void refreshResetButton()
  {
    if (mParent.addingNewObject())
    {
      UiUtils.hide(mReset);
      return;
    }

    if (Editor.nativeIsMapObjectUploaded())
    {
      mReset.setText(R.string.editor_place_doesnt_exist);
      return;
    }

    switch (Editor.nativeGetMapObjectStatus())
    {
      case Editor.CREATED -> mReset.setText(R.string.editor_remove_place_button);
      case Editor.MODIFIED -> mReset.setText(R.string.editor_reset_edits_button);
      case Editor.UNTOUCHED -> mReset.setText(R.string.editor_place_doesnt_exist);
      case Editor.DELETED ->
          throw new IllegalStateException("Can't delete already deleted feature.");
      case Editor.OBSOLETE ->
          throw new IllegalStateException("Obsolete objects cannot be reverted.");
    }
  }

  private void reset()
  {
    if (Editor.nativeIsMapObjectUploaded())
    {
      placeDoesntExist();
      return;
    }

    switch (Editor.nativeGetMapObjectStatus())
    {
      case Editor.CREATED -> rollback(Editor.CREATED);
      case Editor.MODIFIED -> rollback(Editor.MODIFIED);
      case Editor.UNTOUCHED -> placeDoesntExist();
      case Editor.DELETED ->
          throw new IllegalStateException("Can't delete already deleted feature.");
      case Editor.OBSOLETE ->
          throw new IllegalStateException("Obsolete objects cannot be reverted.");
    }
  }

  private void rollback(@Editor.FeatureStatus int status)
  {
    @StringRes final int title;
    @StringRes final int message;
    if (status == Editor.CREATED)
    {
      title = R.string.editor_remove_place_button;
      message = R.string.editor_remove_place_message;
    }
    else
    {
      title = R.string.editor_reset_edits_button;
      message = R.string.editor_reset_edits_message;
    }

    new MaterialAlertDialogBuilder(requireActivity(), R.style.MwmTheme_AlertDialog)
        .setTitle(message)
        .setPositiveButton(title, (dialog, which) -> {
          Editor.nativeRollbackMapObject();
          Framework.nativePokeSearchInViewport();
          mParent.onBackPressed();
        })
        .setNegativeButton(R.string.cancel, null)
        .show();
  }

  private void placeDoesntExist()
  {
    EditTextDialogFragment dialogFragment =
        EditTextDialogFragment.show(getString(R.string.editor_place_doesnt_exist),
                                    "",
                                    getString(R.string.editor_comment_hint),
                                    getString(R.string.editor_report_problem_send_button),
                                    getString(R.string.cancel),
                                    this,
                                    getDeleteCommentValidator());
    dialogFragment.setTextSaveListener(this::commitPlaceDoesntExists);
  }

  private void commitPlaceDoesntExists(@NonNull String text)
  {
    Editor.nativePlaceDoesNotExist(text);
    mParent.onBackPressed();
  }

  @NonNull
  private EditTextDialogFragment.Validator getDeleteCommentValidator()
  {
    return (activity, text) -> {
      if (TextUtils.isEmpty(text))
        return new Option<>(activity.getString(R.string.delete_place_empty_comment_error));
      else
        return Option.empty();
    };
  }
}
