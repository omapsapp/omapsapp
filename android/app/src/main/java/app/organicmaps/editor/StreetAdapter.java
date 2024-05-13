package app.organicmaps.editor;

import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CompoundButton;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import app.organicmaps.MwmApplication;
import app.organicmaps.R;
import app.organicmaps.dialog.EditTextDialogFragment;
import app.organicmaps.sdk.editor.data.LocalizedStreet;
import app.organicmaps.util.UiUtils;

public class StreetAdapter extends RecyclerView.Adapter<StreetAdapter.BaseViewHolder>
{
  private static final int TYPE_ADD_STREET = 0;
  private static final int TYPE_STREET = 1;

  private final LocalizedStreet[] mStreets;
  private final StreetFragment mFragment;
  private LocalizedStreet mSelectedStreet;

  public StreetAdapter(@NonNull StreetFragment host, @NonNull LocalizedStreet[] streets, @NonNull LocalizedStreet selected)
  {
    mFragment = host;
    mStreets = streets;
    mSelectedStreet = selected;
  }

  @Override
  public BaseViewHolder onCreateViewHolder(ViewGroup parent, int viewType)
  {
    return viewType == TYPE_STREET ? new StreetViewHolder(LayoutInflater.from(parent.getContext()).inflate(R.layout.item_street, parent, false))
                                   : new AddViewHolder(LayoutInflater.from(parent.getContext()).inflate(R.layout.item_add_street, parent, false));
  }

  @Override
  public void onBindViewHolder(BaseViewHolder holder, int position)
  {
    holder.bind(position);
  }

  @Override
  public int getItemCount()
  {
    return mStreets.length + 1;
  }

  @Override
  public int getItemViewType(int position)
  {
    return position == getItemCount() - 1 ? TYPE_ADD_STREET : TYPE_STREET;
  }

  public LocalizedStreet getSelectedStreet()
  {
    return mSelectedStreet;
  }

  private void addStreet()
  {
    final Resources resources = MwmApplication.from(mFragment.requireContext()).getResources();
    EditTextDialogFragment dialogFragment =
        EditTextDialogFragment.show(resources.getString(R.string.street), null,
                                    resources.getString(R.string.ok),
                                    resources.getString(R.string.cancel),
                                    mFragment,
                                    StreetFragment.getStreetValidator());
    dialogFragment.setTextSaveListener(mFragment.getSaveStreetListener());
  }

  protected abstract class BaseViewHolder extends RecyclerView.ViewHolder
  {
    public BaseViewHolder(View itemView)
    {
      super(itemView);
    }

    public void bind(int position) {}
  }

  protected class StreetViewHolder extends BaseViewHolder implements View.OnClickListener
  {
    final TextView streetDef;
    final TextView streetLoc;
    final CompoundButton selected;

    public StreetViewHolder(View itemView)
    {
      super(itemView);
      streetDef = itemView.findViewById(R.id.street_default);
      streetLoc = itemView.findViewById(R.id.street_localized);
      selected = itemView.findViewById(R.id.selected);
      itemView.setOnClickListener(this);
      selected.setOnClickListener(v -> {
        selected.toggle();
        StreetViewHolder.this.onClick(selected);
      });
    }

    @Override
    public void bind(int position)
    {
      selected.setChecked(mSelectedStreet.defaultName.equals(mStreets[position].defaultName));
      streetDef.setText(mStreets[position].defaultName);
      UiUtils.setTextAndHideIfEmpty(streetLoc, mStreets[position].localizedName);
    }

    @Override
    public void onClick(View v)
    {
      mSelectedStreet = mStreets[getBindingAdapterPosition()];
      notifyDataSetChanged();
      mFragment.saveStreet(mSelectedStreet);
    }
  }

  protected class AddViewHolder extends BaseViewHolder
  {
    public AddViewHolder(View itemView)
    {
      super(itemView);
      itemView.setOnClickListener(v -> addStreet());
    }
  }
}
