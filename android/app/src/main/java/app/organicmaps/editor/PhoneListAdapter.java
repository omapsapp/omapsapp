package app.organicmaps.editor;

import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.textfield.TextInputLayout;
import com.google.android.material.textfield.TextInputEditText;
import app.organicmaps.R;
import app.organicmaps.sdk.editor.Editor;
import app.organicmaps.sdk.util.StringUtils;
import app.organicmaps.util.UiUtils;

import java.util.ArrayList;
import java.util.List;

public class PhoneListAdapter extends RecyclerView.Adapter<PhoneListAdapter.ViewHolder>
{
  private List<String> phonesData = new ArrayList<>();

  public PhoneListAdapter(String phoneList)
  {
    if (TextUtils.isEmpty(phoneList))
    {
      phonesData.add("");
      return;
    }

    phonesData = new ArrayList<>();
    for (String p : phoneList.split(";"))
    {
      p = p.trim();
      if (TextUtils.isEmpty(p)) continue;
      phonesData.add(p);
    }

    if (phonesData.isEmpty()) phonesData.add("");
  }

  @NonNull
  @Override
  public ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType)
  {
    return new ViewHolder(LayoutInflater.from(parent.getContext())
                                        .inflate(R.layout.item_phone, parent, false));
  }

  @Override
  public void onBindViewHolder(@NonNull PhoneListAdapter.ViewHolder holder, int position)
  {
    holder.setPosition(position);
    holder.setPhone(phonesData.get(position));
  }

  @Override
  public int getItemCount()
  {
    return phonesData.size();
  }

  @Override
  public long getItemId(int position)
  {
    return position;
  }

  public void appendPhone()
  {
    phonesData.add("");
    notifyDataSetChanged();
  }

  public String getPhoneList()
  {
    StringBuilder sb = new StringBuilder();
    for (int i = 0; i < phonesData.size(); i++)
    {
      final String p = phonesData.get(i).trim();
      if (!TextUtils.isEmpty(p))
      {
        if (sb.length() > 0)
          sb.append(';');
        sb.append(p);
      }
    }
    return sb.toString();
  }

  protected class ViewHolder extends RecyclerView.ViewHolder implements View.OnClickListener
  {
    private int mPosition = -1;
    private final TextInputEditText mInput;
    private final ImageView deleteButton;

    public ViewHolder(@NonNull View itemView)
    {
      super(itemView);

      mInput = itemView.findViewById(R.id.input);
      final TextInputLayout phoneInput = itemView.findViewById(R.id.phone_input);
      mInput.addTextChangedListener(new StringUtils.SimpleTextWatcher()
      {
        @Override
        public void onTextChanged(CharSequence s, int start, int before, int count)
        {
          UiUtils.setInputError(phoneInput, Editor.nativeIsPhoneValid(s.toString()) ? 0 : R.string.error_enter_correct_phone);
          PhoneListAdapter.this.updatePhoneItem(mPosition, mInput.getText().toString());
        }
      });

      deleteButton = itemView.findViewById(R.id.delete_icon);
      deleteButton.setOnClickListener(this);
      // TODO: setting icons from code because icons defined in layout XML are white.
      deleteButton.setImageResource(R.drawable.ic_delete);
      ((ImageView) itemView.findViewById(R.id.phone_icon)).setImageResource(R.drawable.ic_phone);
    }

    public void setPosition(int position)
    {
      mPosition = position;
    }

    public void setPhone(String phone)
    {
      if (!mInput.getText().toString().equals(phone))
        mInput.setText(phone);
    }

    @Override
    public void onClick(View view)
    {
      if (view.getId() == R.id.delete_icon)
        PhoneListAdapter.this.deleteItem(mPosition);
    }
  }

  private void updatePhoneItem(int position, String text)
  {
    if (position == -1) return;
    phonesData.set(position, text);
  }

  void deleteItem(int position)
  {
    phonesData.remove(position);
    notifyDataSetChanged();
  }
}
