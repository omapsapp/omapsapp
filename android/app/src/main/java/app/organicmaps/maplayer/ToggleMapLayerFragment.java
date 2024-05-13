package app.organicmaps.maplayer;

import android.content.Context;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.ViewModelProvider;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import app.organicmaps.R;
import app.organicmaps.sdk.maplayer.isolines.IsolinesManager;
import app.organicmaps.util.SharedPropertiesUtils;
import app.organicmaps.util.Utils;
import app.organicmaps.util.bottomsheet.MenuBottomSheetFragment;
import app.organicmaps.widget.recycler.SpanningLinearLayoutManager;
import com.google.android.material.button.MaterialButton;

import java.util.ArrayList;
import java.util.List;

public class ToggleMapLayerFragment extends Fragment
{
  private static final String LAYERS_MENU_ID = "LAYERS_MENU_BOTTOM_SHEET";
  @Nullable
  private LayersAdapter mAdapter;
  private MapButtonsViewModel mMapButtonsViewModel;

  @Nullable
  @Override
  public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState)
  {
    View mRoot = inflater.inflate(R.layout.fragment_toggle_map_layer, container, false);

    mMapButtonsViewModel = new ViewModelProvider(requireActivity()).get(MapButtonsViewModel.class);
    MaterialButton mCloseButton = mRoot.findViewById(R.id.close_button);
    mCloseButton.setOnClickListener(view -> closeLayerBottomSheet());

    initRecycler(mRoot);
    return mRoot;
  }

  private void initRecycler(@NonNull View root)
  {
    RecyclerView recycler = root.findViewById(R.id.recycler);
    RecyclerView.LayoutManager layoutManager = new SpanningLinearLayoutManager(requireContext(),
        LinearLayoutManager.HORIZONTAL,
        false);
    recycler.setLayoutManager(layoutManager);
    mAdapter = new LayersAdapter(getLayersItems());
    recycler.setAdapter(mAdapter);
    recycler.setNestedScrollingEnabled(false);
  }

  private List<LayerBottomSheetItem> getLayersItems()
  {
    List<Mode> availableLayers = LayersUtils.getAvailableLayers();
    List<LayerBottomSheetItem> items = new ArrayList<>();
    for (Mode layer : availableLayers)
    {
      items.add(LayerBottomSheetItem.create(requireContext(), layer, this::onItemClick));
    }
    return items;
  }

  private void onItemClick(@NonNull View v, @NonNull LayerBottomSheetItem item)
  {
    Mode mode = item.getMode();
    Context context = v.getContext();
    SharedPropertiesUtils.setLayerMarkerShownForLayerMode(context, mode);
    mode.setEnabled(context, !mode.isEnabled(context));
    mAdapter.notifyDataSetChanged();
    if (IsolinesManager.from(context).shouldShowNotification())
      Utils.showSnackbar(context, v.getRootView(), R.string.isolines_toast_zooms_1_10);
  }

  private void closeLayerBottomSheet()
  {
    MenuBottomSheetFragment bottomSheet =
        (MenuBottomSheetFragment) requireActivity().getSupportFragmentManager().findFragmentByTag(LAYERS_MENU_ID);
    if (bottomSheet != null)
      bottomSheet.dismiss();
  }
}
