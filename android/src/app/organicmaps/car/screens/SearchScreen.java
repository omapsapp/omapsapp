package app.organicmaps.car.screens;

import androidx.annotation.NonNull;
import androidx.car.app.CarContext;
import androidx.car.app.Screen;
import androidx.car.app.constraints.ConstraintManager;
import androidx.car.app.model.Action;
import androidx.car.app.model.CarIcon;
import androidx.car.app.model.ItemList;
import androidx.car.app.model.Row;
import androidx.car.app.model.SearchTemplate;
import androidx.car.app.model.Template;
import androidx.core.graphics.drawable.IconCompat;

import app.organicmaps.R;
import app.organicmaps.search.SearchRecents;

public class SearchScreen extends Screen implements SearchTemplate.SearchCallback
{
  private final int MAX_RESULTS_SIZE;
  private ItemList mResults;
  private String mSearchText = "";

  public SearchScreen(@NonNull CarContext carContext)
  {
    super(carContext);
    final ConstraintManager constraintManager = getCarContext().getCarService(ConstraintManager.class);
    MAX_RESULTS_SIZE = constraintManager.getContentLimit(ConstraintManager.CONTENT_LIMIT_TYPE_LIST);
  }

  @NonNull
  @Override
  public Template onGetTemplate()
  {
    SearchTemplate.Builder builder = new SearchTemplate.Builder(this);
    builder.setHeaderAction(Action.BACK);
    builder.setShowKeyboardByDefault(false);
    if (mSearchText.isEmpty() || mResults == null)
      loadRecents();
    builder.setItemList(mResults);
    builder.setInitialSearchText(mSearchText);
    return builder.build();
  }

  private void loadRecents()
  {
    final CarIcon iconRecent = new CarIcon.Builder(IconCompat.createWithResource(getCarContext(), R.drawable.ic_search_recent)).build();

    ItemList.Builder builder = new ItemList.Builder();
    builder.setNoItemsMessage(getCarContext().getString(R.string.search_history_text));
    SearchRecents.refresh();
    int recentsSize = Math.min(SearchRecents.getSize(), MAX_RESULTS_SIZE);
    for (int i = 0; i < recentsSize; ++i)
    {
      Row.Builder itemBuilder = new Row.Builder();
      itemBuilder.setTitle(SearchRecents.get(i));
      itemBuilder.setImage(iconRecent);
      builder.addItem(itemBuilder.build());
    }
    mResults = builder.build();
  }
}
