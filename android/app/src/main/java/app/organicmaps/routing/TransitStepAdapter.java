package app.organicmaps.routing;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import app.organicmaps.R;
import app.organicmaps.sdk.routing.TransitStepInfo;

import java.util.ArrayList;
import java.util.List;

public class TransitStepAdapter extends RecyclerView.Adapter<TransitStepAdapter.TransitStepViewHolder>
{
  @NonNull
  private final List<TransitStepInfo> mItems = new ArrayList<>();

  @Override
  public TransitStepViewHolder onCreateViewHolder(ViewGroup parent, int viewType)
  {
    return new TransitStepViewHolder(LayoutInflater.from(parent.getContext())
                                                   .inflate(R.layout.routing_transit_step_view, parent, false));
  }

  @Override
  public void onBindViewHolder(TransitStepViewHolder holder, int position)
  {
    holder.bind(mItems.get(position));
  }

  @Override
  public int getItemCount()
  {
    return mItems.size();
  }

  public void setItems(@NonNull List<TransitStepInfo> items)
  {
    mItems.clear();
    mItems.addAll(items);
    notifyDataSetChanged();
  }

  static class TransitStepViewHolder extends RecyclerView.ViewHolder
  {
    @NonNull
    private final TransitStepView mView;

    TransitStepViewHolder(@NonNull View itemView)
    {
      super(itemView);
      mView = (TransitStepView) itemView;
    }

    void bind(@NonNull TransitStepInfo info)
    {
      mView.setTransitStepInfo(info);
    }
  }
}
