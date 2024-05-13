package app.organicmaps.editor;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.textfield.TextInputLayout;
import com.google.android.material.textfield.TextInputEditText;
import app.organicmaps.R;
import app.organicmaps.sdk.editor.Editor;
import app.organicmaps.sdk.editor.data.Language;
import app.organicmaps.sdk.editor.data.LocalizedName;
import app.organicmaps.sdk.util.StringUtils;
import app.organicmaps.util.UiUtils;
import app.organicmaps.util.Utils;

import java.util.List;

public class MultilanguageAdapter extends RecyclerView.Adapter<MultilanguageAdapter.Holder>
{
  private final List<LocalizedName> mNames;
  private final int mMandatoryNamesCount;
  private boolean mAdditionalLanguagesShown;

  MultilanguageAdapter(EditorHostFragment hostFragment)
  {
    mNames = hostFragment.getNames();
    mMandatoryNamesCount = hostFragment.getMandatoryNamesCount();
  }

  @Override
  public Holder onCreateViewHolder(ViewGroup parent, int viewType)
  {
    final View view = LayoutInflater.from(parent.getContext()).inflate(R.layout.item_localized_name, parent, false);
    // TODO(mgsergio): Deletion is not implemented.
    UiUtils.hide(view.findViewById(R.id.delete));
    return new Holder(view);
  }

  @Override
  public void onBindViewHolder(Holder holder, int position)
  {
    LocalizedName name = mNames.get(position);
    holder.input.setText(name.name);
    if (name.lang.equals(Language.DEFAULT_LANG_CODE))
      holder.inputLayout.setHint(holder.itemView.getContext().getString(R.string.editor_default_language_hint));
    else
      holder.inputLayout.setHint(name.langName);
  }

  @Override
  public int getItemCount()
  {
    return mAdditionalLanguagesShown ? mNames.size() : mMandatoryNamesCount;
  }

  public int getNamesCount()
  {
    return mNames.size();
  }

  @NonNull
  LocalizedName getNameAtPos(int pos) { return mNames.get(pos); }

  public int getMandatoryNamesCount()
  {
    return mMandatoryNamesCount;
  }

  public boolean areAdditionalLanguagesShown()
  {
    return mAdditionalLanguagesShown;
  }

  public void showAdditionalLanguages(boolean show)
  {
    if (mAdditionalLanguagesShown == show)
      return;

    mAdditionalLanguagesShown = show;

    if (mNames.size() != mMandatoryNamesCount)
    {
      if (show)
      {
        notifyItemRangeInserted(mMandatoryNamesCount, mNames.size() - mMandatoryNamesCount);
      }
      else
      {
        notifyItemRangeRemoved(mMandatoryNamesCount, mNames.size() - mMandatoryNamesCount);
      }
    }
  }

  public class Holder extends RecyclerView.ViewHolder
  {
    TextInputEditText input;
    TextInputLayout inputLayout;

    public Holder(View itemView)
    {
      super(itemView);
      input = itemView.findViewById(R.id.input);
      inputLayout = itemView.findViewById(R.id.input_layout);
      input.addTextChangedListener(new StringUtils.SimpleTextWatcher()
      {
        @Override
        public void onTextChanged(CharSequence s, int start, int before, int count)
        {
          UiUtils.setInputError(inputLayout, Editor.nativeIsNameValid(s.toString()) ?
                                                                      Utils.INVALID_ID :
                                                                      R.string.error_enter_correct_name);
          mNames.get(getBindingAdapterPosition()).name = s.toString();
        }
      });

      itemView.findViewById(R.id.delete).setOnClickListener(v -> {
        // TODO(mgsergio): Implement item deletion.
        // int position = getAdapterPosition();
        // mHostFragment.removeLocalizedName(position + 1);
        // mNames.remove(position);
        // notifyItemRemoved(position);
      });
    }
  }
}
