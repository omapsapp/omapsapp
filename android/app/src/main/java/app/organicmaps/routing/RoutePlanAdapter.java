package app.organicmaps.routing;

import android.content.Context;
import android.content.res.TypedArray;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.RecyclerView;

import app.organicmaps.R;
import app.organicmaps.util.DateUtils;
import app.organicmaps.util.UiUtils;

public class RoutePlanAdapter extends RecyclerView.Adapter<RoutePlanAdapter.RoutePlanViewHolder>
{
  Context mContext;
  RouteMarkData[] mRouteMarkData;

  public RoutePlanAdapter(Context context, RouteMarkData[] routeMarkData)
  {
    mContext = context;

    // Copy all route points, except the first one (starting point).
    mRouteMarkData = new RouteMarkData[routeMarkData.length - 1];
    System.arraycopy(routeMarkData, 1, mRouteMarkData, 0, routeMarkData.length - 1);
  }

  @NonNull
  @Override
  public RoutePlanViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType)
  {
    View view = LayoutInflater.from(parent.getContext()).inflate(R.layout.route_plan_list_item,
                                                                 parent, false);

    return new RoutePlanViewHolder(view);
  }

  @Override
  public void onBindViewHolder(@NonNull RoutePlanViewHolder holder, int position)
  {
    int markPos = mRouteMarkData.length - 1 - position;
    int iconId;

    if (mRouteMarkData[markPos].mPointType == 0)
    {
      // Start point.
      iconId = R.drawable.route_point_start;
    }
    else if (mRouteMarkData[markPos].mPointType == 1)
    {
      // Intermediate stop.
      TypedArray iconArray = mContext.getResources().obtainTypedArray(R.array.route_stop_icons);
      iconId = iconArray.getResourceId(mRouteMarkData[markPos].mIntermediateIndex,
              R.drawable.route_point_01);
    }
    else
    {
      // Finish point.
      iconId = R.drawable.route_point_finish;
    }

    holder.mImageViewIcon.setImageDrawable(AppCompatResources.getDrawable(mContext, iconId));

    holder.mTextViewEta.setText(DateUtils.getEstimateTimeString(mContext,
                                mRouteMarkData[markPos].mTimeSec));

    if (mRouteMarkData[markPos].mPointType == 0)
    {
      UiUtils.hide(holder.mTextViewSeparator1);
      UiUtils.hide(holder.mTextViewTime);
      UiUtils.hide(holder.mTextViewSeparator2);
      UiUtils.hide(holder.mTextViewDistance);
    }
    else
    {
      holder.mTextViewTime.setText(DateUtils.getRemainingTimeString(mContext,
                                   mRouteMarkData[markPos].mTimeSec));

      holder.mTextViewDistance.setText(mRouteMarkData[markPos].mDistance);
    }
  }

  @Override
  public int getItemCount()
  {
    return mRouteMarkData.length;
  }

  static class RoutePlanViewHolder extends RecyclerView.ViewHolder
  {
    @NonNull
    public final ImageView mImageViewIcon;

    @NonNull
    public final TextView mTextViewEta;

    @NonNull
    public final TextView mTextViewSeparator1;

    @NonNull
    public final TextView mTextViewTime;

    @NonNull
    public final TextView mTextViewSeparator2;

    @NonNull
    public final TextView mTextViewDistance;

    RoutePlanViewHolder(@NonNull View itemView)
    {
      super(itemView);
      mImageViewIcon = itemView.findViewById(R.id.icon);
      mTextViewEta = itemView.findViewById(R.id.eta);
      mTextViewSeparator1 = itemView.findViewById(R.id.separator1);
      mTextViewTime = itemView.findViewById(R.id.time);
      mTextViewSeparator2 = itemView.findViewById(R.id.separator2);
      mTextViewDistance = itemView.findViewById(R.id.distance);
    }
  }
}
