package com.mapswithme.maps.editor;

import android.content.DialogInterface;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.text.InputType;
import android.text.TextUtils;
import android.util.SparseArray;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.CallSuper;
import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.widget.SwitchCompat;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.textfield.TextInputLayout;
import com.mapswithme.maps.Framework;
import com.mapswithme.maps.R;
import com.mapswithme.maps.base.BaseMwmFragment;
import com.mapswithme.maps.bookmarks.data.MapObject.OsmProps;
import com.mapswithme.maps.dialog.EditTextDialogFragment;
import com.mapswithme.maps.editor.data.LocalizedName;
import com.mapswithme.maps.editor.data.LocalizedStreet;
import com.mapswithme.maps.editor.data.TimeFormatUtils;
import com.mapswithme.maps.editor.data.Timetable;
import com.mapswithme.util.Constants;
import com.mapswithme.util.Graphics;
import com.mapswithme.util.InputUtils;
import com.mapswithme.util.StringUtils;
import com.mapswithme.util.UiUtils;
import com.mapswithme.util.Utils;

public class EditorFragment extends BaseMwmFragment implements View.OnClickListener,
                                                               EditTextDialogFragment.EditTextDialogInterface
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
  private EditText mHouseNumber;
  private EditText mZipcode;
  private View mBlockLevels;
  private EditText mBuildingLevels;
  private TextView mPhone;
  private TextView mEditPhoneLink;
  private EditText mWebsite;
  private EditText mEmail;
  private EditText mFacebookPage;
  private EditText mInstagramPage;
  private EditText mTwitterPage;
  private EditText mVkPage;
  private TextView mCuisine;
  private EditText mOperator;
  private SwitchCompat mWifi;

  private TextInputLayout mInputHouseNumber;
  private TextInputLayout mInputBuildingLevels;
  private TextInputLayout mInputZipcode;
  private TextInputLayout mInputWebsite;
  private TextInputLayout mInputEmail;
  private TextInputLayout mInputFacebookPage;
  private TextInputLayout mInputInstagramPage;
  private TextInputLayout mInputTwitterPage;
  private TextInputLayout mInputVkPage;

  private View mEmptyOpeningHours;
  private TextView mOpeningHours;
  private View mEditOpeningHours;
  private EditText mDescription;
  private final SparseArray<View> mDetailsBlocks = new SparseArray<>(7);
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

    mCategory.setText(Utils.getLocalizedFeatureType(getContext(), Editor.nativeGetCategory()));
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

    mZipcode.setText(Editor.nativeGetZipCode());
    mZipcode.addTextChangedListener(new StringUtils.SimpleTextWatcher()
    {
      @Override
      public void onTextChanged(CharSequence s, int start, int before, int count)
      {
        UiUtils.setInputError(mInputZipcode, Editor.nativeIsZipcodeValid(s.toString()) ? 0 : R.string.error_enter_correct_zip_code);
      }
    });

    mBuildingLevels.setText(Editor.nativeGetBuildingLevels());
    mBuildingLevels.addTextChangedListener(new StringUtils.SimpleTextWatcher()
    {
      @Override
      public void onTextChanged(CharSequence s, int start, int before, int count)
      {
        UiUtils.setInputError(mInputBuildingLevels, Editor.nativeIsLevelValid(s.toString()) ? 0 : R.string.error_enter_correct_storey_number);
      }
    });

    mPhone.setText(Editor.nativeGetPhone());

    mWebsite.setText(Editor.nativeGetWebsite());
    mWebsite.addTextChangedListener(new StringUtils.SimpleTextWatcher()
    {
      @Override
      public void onTextChanged(CharSequence s, int start, int before, int count)
      {
        UiUtils.setInputError(mInputWebsite, Editor.nativeIsWebsiteValid(s.toString()) ? 0 : R.string.error_enter_correct_web);
      }
    });

    mEmail.setText(Editor.nativeGetEmail());
    mEmail.addTextChangedListener(new StringUtils.SimpleTextWatcher()
    {
      @Override
      public void onTextChanged(CharSequence s, int start, int before, int count)
      {
        UiUtils.setInputError(mInputEmail, Editor.nativeIsEmailValid(s.toString()) ? 0 : R.string.error_enter_correct_email);
      }
    });

    mFacebookPage.setText(Editor.nativeGetFacebookPage());
    mFacebookPage.addTextChangedListener(new StringUtils.SimpleTextWatcher()
    {
      @Override
      public void onTextChanged(CharSequence s, int start, int before, int count)
      {
        UiUtils.setInputError(mInputFacebookPage, Editor.nativeIsFacebookPageValid(s.toString()) ? 0 : R.string.error_enter_correct_web);
      }
    });

    mInstagramPage.setText(Editor.nativeGetInstagramPage());
    mInstagramPage.addTextChangedListener(new StringUtils.SimpleTextWatcher()
    {
      @Override
      public void onTextChanged(CharSequence s, int start, int before, int count)
      {
        UiUtils.setInputError(mInputInstagramPage, Editor.nativeIsInstagramPageValid(s.toString()) ? 0 : R.string.error_enter_correct_web);
      }
    });

    mTwitterPage.setText(Editor.nativeGetTwitterPage());
    mTwitterPage.addTextChangedListener(new StringUtils.SimpleTextWatcher()
    {
      @Override
      public void onTextChanged(CharSequence s, int start, int before, int count)
      {
        UiUtils.setInputError(mInputTwitterPage, Editor.nativeIsTwitterPageValid(s.toString()) ? 0 : R.string.error_enter_correct_web);
      }
    });

    mVkPage.setText(Editor.nativeGetVkPage());
    mVkPage.addTextChangedListener(new StringUtils.SimpleTextWatcher()
    {
      @Override
      public void onTextChanged(CharSequence s, int start, int before, int count)
      {
        UiUtils.setInputError(mInputVkPage, Editor.nativeIsVkPageValid(s.toString()) ? 0 : R.string.error_enter_correct_web);
      }
    });

    mCuisine.setText(Editor.nativeGetFormattedCuisine());
    mOperator.setText(Editor.nativeGetOperator());
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
    Editor.nativeSetZipCode(mZipcode.getText().toString());
    Editor.nativeSetBuildingLevels(mBuildingLevels.getText().toString());
    Editor.nativeSetWebsite(mWebsite.getText().toString());
    Editor.nativeSetEmail(mEmail.getText().toString());
    Editor.nativeSetFacebookPage(mFacebookPage.getText().toString());
    Editor.nativeSetInstagramPage(mInstagramPage.getText().toString());
    Editor.nativeSetTwitterPage(mTwitterPage.getText().toString());
    Editor.nativeSetVkPage(mVkPage.getText().toString());
    Editor.nativeSetHasWifi(mWifi.isChecked());
    Editor.nativeSetOperator(mOperator.getText().toString());
    Editor.nativeSetNames(mParent.getNamesAsArray());

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

    if (!Editor.nativeIsZipcodeValid(mZipcode.getText().toString()))
    {
      mZipcode.requestFocus();
      InputUtils.showKeyboard(mZipcode);
      return false;
    }

    if (!Editor.nativeIsPhoneValid(mPhone.getText().toString()))
    {
      mPhone.requestFocus();
      InputUtils.showKeyboard(mPhone);
      return false;
    }

    if (!Editor.nativeIsWebsiteValid(mWebsite.getText().toString()))
    {
      mWebsite.requestFocus();
      InputUtils.showKeyboard(mWebsite);
      return false;
    }

    if (!Editor.nativeIsEmailValid(mEmail.getText().toString()))
    {
      mEmail.requestFocus();
      InputUtils.showKeyboard(mEmail);
      return false;
    }

    if (!Editor.nativeIsFacebookPageValid(mFacebookPage.getText().toString()))
    {
      mFacebookPage.requestFocus();
      InputUtils.showKeyboard(mFacebookPage);
      return false;
    }

    if (!Editor.nativeIsWebsiteValid(mInstagramPage.getText().toString()))
    {
      mInstagramPage.requestFocus();
      InputUtils.showKeyboard(mInstagramPage);
      return false;
    }

    if (!Editor.nativeIsWebsiteValid(mTwitterPage.getText().toString()))
    {
      mTwitterPage.requestFocus();
      InputUtils.showKeyboard(mTwitterPage);
      return false;
    }

    if (!Editor.nativeIsWebsiteValid(mVkPage.getText().toString()))
    {
      mVkPage.requestFocus();
      InputUtils.showKeyboard(mVkPage);
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

    for (int i = 0; i < mDetailsBlocks.size(); i++)
      UiUtils.hide(mDetailsBlocks.valueAt(i));

    boolean anyEditableDetails = false;
    for (int type : editableDetails)
    {
      final View detailsBlock = mDetailsBlocks.get(type);
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
                                          : TimeFormatUtils.formatTimetables(requireContext(),
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
    mNamesView.setLayoutManager(new LinearLayoutManager(getActivity()));
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
    UiUtils.waitLayout(mNamesView, new ViewTreeObserver.OnGlobalLayoutListener()
    {
      @Override
      public void onGlobalLayout()
      {
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
      }
    });
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
    View blockZipcode = view.findViewById(R.id.block_zipcode);
    mZipcode = findInputAndInitBlock(blockZipcode, 0, R.string.editor_zip_code);
    mInputZipcode = blockZipcode.findViewById(R.id.custom_input);

    // Details
    mBlockLevels = view.findViewById(R.id.block_levels);
    mBuildingLevels = findInputAndInitBlock(mBlockLevels, 0, getString(R.string.editor_storey_number, 25));
    mBuildingLevels.setInputType(InputType.TYPE_CLASS_NUMBER);
    mInputBuildingLevels = mBlockLevels.findViewById(R.id.custom_input);
    View blockPhone = view.findViewById(R.id.block_phone);
    mPhone = blockPhone.findViewById(R.id.phone);
    mEditPhoneLink = blockPhone.findViewById(R.id.edit_phone);
    mEditPhoneLink.setOnClickListener(this);
    mPhone.setOnClickListener(this);
    View blockWeb = view.findViewById(R.id.block_website);
    mWebsite = findInputAndInitBlock(blockWeb, R.drawable.ic_website, R.string.website);
    mWebsite.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
    mInputWebsite = blockWeb.findViewById(R.id.custom_input);
    View blockEmail = view.findViewById(R.id.block_email);
    mEmail = findInputAndInitBlock(blockEmail, R.drawable.ic_email, R.string.email);
    mEmail.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS);
    mInputEmail = blockEmail.findViewById(R.id.custom_input);

    View blockFacebookPage = view.findViewById(R.id.block_facebook);
    mFacebookPage = findInputAndInitBlock(blockFacebookPage, R.drawable.ic_facebook, R.string.facebook);
    mFacebookPage.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
    mInputFacebookPage = blockFacebookPage.findViewById(R.id.custom_input);

    View blockInstagramPage = view.findViewById(R.id.block_instagram);
    mInstagramPage = findInputAndInitBlock(blockInstagramPage, R.drawable.ic_instagram, R.string.instagram);
    mInstagramPage.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
    mInputInstagramPage = blockInstagramPage.findViewById(R.id.custom_input);

    View blockTwitterPage = view.findViewById(R.id.block_twitter);
    mTwitterPage = findInputAndInitBlock(blockTwitterPage, R.drawable.ic_twitter, R.string.twitter);
    mTwitterPage.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
    mInputTwitterPage = blockTwitterPage.findViewById(R.id.custom_input);

    View blockVkPage = view.findViewById(R.id.block_vk);
    mVkPage = findInputAndInitBlock(blockVkPage, R.drawable.ic_vk, R.string.vk);
    mVkPage.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
    mInputVkPage = blockVkPage.findViewById(R.id.custom_input);

    View blockCuisine = view.findViewById(R.id.block_cuisine);
    blockCuisine.setOnClickListener(this);
    mCuisine = view.findViewById(R.id.cuisine);
    View blockOperator = view.findViewById(R.id.block_operator);
    mOperator = findInputAndInitBlock(blockOperator, R.drawable.ic_operator, R.string.editor_operator);
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

    mDetailsBlocks.append(OsmProps.OpeningHours.ordinal(), blockOpeningHours);
    mDetailsBlocks.append(OsmProps.Phone.ordinal(), blockPhone);
    mDetailsBlocks.append(OsmProps.Website.ordinal(), blockWeb);
    mDetailsBlocks.append(OsmProps.Email.ordinal(), blockEmail);
    mDetailsBlocks.append(OsmProps.Cuisine.ordinal(), blockCuisine);
    mDetailsBlocks.append(OsmProps.Operator.ordinal(), blockOperator);
    mDetailsBlocks.append(OsmProps.Internet.ordinal(), blockWifi);
  }

  private static EditText findInput(View blockWithInput)
  {
    return (EditText) blockWithInput.findViewById(R.id.input);
  }

  private EditText findInputAndInitBlock(View blockWithInput, @DrawableRes int icon, @StringRes int hint)
  {
    return findInputAndInitBlock(blockWithInput, icon, getString(hint));
  }

  private static EditText findInputAndInitBlock(View blockWithInput, @DrawableRes int icon, String hint)
  {
    ((ImageView) blockWithInput.findViewById(R.id.icon)).setImageResource(icon);
    final TextInputLayout input = blockWithInput.findViewById(R.id.custom_input);
    input.setHint(hint);
    return (EditText) input.findViewById(R.id.input);
  }

  @Override
  public void onClick(View v)
  {
    switch (v.getId())
    {
    case R.id.edit_opening_hours:
    case R.id.empty_opening_hours:
    case R.id.opening_hours:
      mParent.editTimetable();
      break;
    case R.id.phone:
    case R.id.edit_phone:
      mParent.editPhone();
    case R.id.block_wifi:
      mWifi.toggle();
      break;
    case R.id.block_street:
      mParent.editStreet();
      break;
    case R.id.block_cuisine:
      mParent.editCuisine();
      break;
    case R.id.category:
      mParent.editCategory();
      break;
    case R.id.more_names:
    case R.id.show_additional_names:
      if (mNamesAdapter.areAdditionalLanguagesShown() && !validateNames())
        break;
      showAdditionalNames(!mNamesAdapter.areAdditionalLanguagesShown());
      break;
    case R.id.add_langs:
      mParent.addLanguage();
      break;
    case R.id.about_osm:
      startActivity(new Intent((Intent.ACTION_VIEW), Uri.parse(Constants.Url.OSM_ABOUT)));
      break;
    case R.id.reset:
      reset();
      break;
    }
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
      Graphics.tint(getActivity(), arrowResourceId, R.attr.iconTint),
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
    case Editor.CREATED:
      mReset.setText(R.string.editor_remove_place_button);
      break;
    case Editor.MODIFIED:
      mReset.setText(R.string.editor_reset_edits_button);
      break;
    case Editor.UNTOUCHED:
      mReset.setText(R.string.editor_place_doesnt_exist);
      break;
    case Editor.DELETED:
      throw new IllegalStateException("Can't delete already deleted feature.");
    case Editor.OBSOLETE:
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
    case Editor.CREATED:
      rollback(Editor.CREATED);
      break;
    case Editor.MODIFIED:
      rollback(Editor.MODIFIED);
      break;
    case Editor.UNTOUCHED:
      placeDoesntExist();
      break;
    case Editor.DELETED:
      throw new IllegalStateException("Can't delete already deleted feature.");
    case Editor.OBSOLETE:
      throw new IllegalStateException("Obsolete objects cannot be reverted.");
    }
  }

  private void rollback(@Editor.FeatureStatus int status)
  {
    int title;
    int message;
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

    new AlertDialog.Builder(getActivity()).setTitle(message)
                                          .setPositiveButton(getString(title).toUpperCase(), new DialogInterface.OnClickListener()
                                          {
                                            @Override
                                            public void onClick(DialogInterface dialog, int which)
                                            {
                                              Editor.nativeRollbackMapObject();
                                              Framework.nativePokeSearchInViewport();
                                              mParent.onBackPressed();
                                            }
                                          })
                                          .setNegativeButton(getString(R.string.cancel).toUpperCase(), null)
                                          .show();
  }

  private void placeDoesntExist()
  {
    EditTextDialogFragment.show(getString(R.string.editor_place_doesnt_exist), "", getString(R.string.editor_comment_hint),
                                getString(R.string.editor_report_problem_send_button), getString(R.string.cancel), this);
  }

  @NonNull
  @Override
  public EditTextDialogFragment.OnTextSaveListener getSaveTextListener()
  {
    return text -> {
      Editor.nativePlaceDoesNotExist(text);
      mParent.onBackPressed();
    };
  }

  @NonNull
  @Override
  public EditTextDialogFragment.Validator getValidator()
  {
    return (activity, text) -> !TextUtils.isEmpty(text);
  }
}
