package app.organicmaps.editor.data;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import app.organicmaps.R;
import app.organicmaps.base.BaseMwmFragment;
import app.organicmaps.editor.PhoneListAdapter;
import app.organicmaps.util.InputUtils;
import com.google.android.material.floatingactionbutton.FloatingActionButton;

public class PhoneFragment extends BaseMwmFragment implements View.OnClickListener
{
  public static final String EXTRA_PHONE_LIST = "Phone";
  private PhoneListAdapter mAdapter;
  private RecyclerView mPhonesRecycler;
  final Integer maxAdapter = 5;

  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
  {
    return inflater.inflate(R.layout.fragment_phone, container, false);
  }

  @Override
  public boolean onBackPressed()
  {
    super.onBackPressed();
    InputUtils.hideKeyboard(requireView());
    return true;
  }

  @Override
  public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);

    final Bundle args = getArguments();
    String phoneList = null;
    if (args != null)
      phoneList = args.getString(EXTRA_PHONE_LIST);
    mAdapter = new PhoneListAdapter(phoneList);
    mAdapter.setHasStableIds(true);

    FloatingActionButton phoneFab = view.findViewById(R.id.tv__append_phone);
    phoneFab.setOnClickListener(this);
    mPhonesRecycler = view.findViewById(R.id.phones_recycler);
    LinearLayoutManager manager = new LinearLayoutManager(view.getContext());
    manager.setSmoothScrollbarEnabled(true);
    mPhonesRecycler.setLayoutManager(manager);
    mPhonesRecycler.setAdapter(mAdapter);
    //int last_position = mAdapter.getItemCount() - 1;
    //mPhonesRecycler.requestFocus(last_position);
    mPhonesRecycler.smoothScrollToPosition(mPhonesRecycler.getBottom());
    InputUtils.showKeyboard(requireView());
  }

  public String getPhone()
  {
    return mAdapter != null ? mAdapter.getPhoneList() : null;
  }

  @Override
  public void onClick(View view)
  {
    if (view.getId() == R.id.tv__append_phone)
    {
      if (mAdapter != null && mAdapter.getItemCount() <= maxAdapter) mAdapter.appendPhone();
    }
  }
}
