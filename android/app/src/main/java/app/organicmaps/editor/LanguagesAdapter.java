package app.organicmaps.editor;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import app.organicmaps.R;
import app.organicmaps.sdk.editor.data.Language;

public class LanguagesAdapter extends RecyclerView.Adapter<LanguagesAdapter.Holder>
{
  private final Language[] mLanguages;
  private final LanguagesFragment mFragment;

  public LanguagesAdapter(@NonNull LanguagesFragment host, @NonNull Language[] languages)
  {
    mFragment = host;
    mLanguages = languages;
  }

  @Override
  public Holder onCreateViewHolder(ViewGroup parent, int viewType)
  {
    return new Holder(LayoutInflater.from(parent.getContext()).inflate(R.layout.item_language, parent, false));
  }

  @Override
  public void onBindViewHolder(Holder holder, int position)
  {
    holder.bind(position);
  }

  @Override
  public int getItemCount()
  {
    return mLanguages.length;
  }

  protected class Holder extends RecyclerView.ViewHolder
  {
    TextView name;

    public Holder(View itemView)
    {
      super(itemView);
      name = (TextView) itemView;
      itemView.setOnClickListener(v -> mFragment.onLanguageSelected(mLanguages[getBindingAdapterPosition()]));
    }

    public void bind(int position)
    {
      name.setText(mLanguages[position].name);
    }
  }
}
