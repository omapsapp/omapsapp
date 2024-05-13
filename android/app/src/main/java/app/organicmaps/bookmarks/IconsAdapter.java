package app.organicmaps.bookmarks;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ImageView;

import app.organicmaps.R;
import app.organicmaps.sdk.bookmarks.data.Icon;
import app.organicmaps.util.Graphics;

import java.util.List;

public class IconsAdapter extends ArrayAdapter<Icon>
{
  private int mCheckedIconColor;

  public IconsAdapter(Context context, List<Icon> list)
  {
    super(context, 0, 0, list);
  }

  @Override
  public View getView(int position, View convertView, ViewGroup parent)
  {
    SpinnerViewHolder holder;
    if (convertView == null)
    {
      LayoutInflater inflater = LayoutInflater.from(getContext());
      convertView = inflater.inflate(R.layout.color_row, parent, false);
      holder = new SpinnerViewHolder(convertView);
      convertView.setTag(holder);
    }
    else
      holder = (SpinnerViewHolder) convertView.getTag();

    final Icon icon = getItem(position);

    Drawable circle;
    if (icon.getColor() == mCheckedIconColor)
    {
      circle = Graphics.drawCircleAndImage(getItem(position).argb(),
                                           R.dimen.track_circle_size,
                                           app.organicmaps.sdk.R.drawable.ic_bookmark_none,
                                           R.dimen.bookmark_icon_size,
                                           getContext());

    }
    else
    {
      circle = Graphics.drawCircle(getItem(position).argb(),
                                   R.dimen.select_color_circle_size,
                                   getContext().getResources());
    }
    holder.icon.setImageDrawable(circle);
    return convertView;
  }

  private static class SpinnerViewHolder
  {
    final ImageView icon;

    SpinnerViewHolder(View convertView)
    {
      icon = convertView.findViewById(R.id.iv__color);
    }
  }

  public void chooseItem(int position)
  {
    mCheckedIconColor = position;
    notifyDataSetChanged();
  }
}
