package app.organicmaps.editor;

import android.os.Bundle;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import app.organicmaps.R;
import app.organicmaps.base.BaseMwmToolbarFragment;
import app.organicmaps.sdk.editor.Editor;
import app.organicmaps.util.UiUtils;
import com.google.android.material.textfield.TextInputEditText;

public class ReportFragment extends BaseMwmToolbarFragment implements View.OnClickListener
{
  private View mSimpleProblems;
  private View mAdvancedProblem;
  private View mSave;
  private TextInputEditText mProblemInput;

  private boolean mAdvancedMode;

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState)
  {
    return inflater.inflate(R.layout.fragment_report, container, false);
  }

  @Override
  public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);
    getToolbarController().setTitle(R.string.editor_report_problem_title);

    mSave = getToolbarController().getToolbar().findViewById(R.id.save);
    mSave.setOnClickListener(this);
    mSimpleProblems = view.findViewById(R.id.ll__problems);
    mSimpleProblems.findViewById(R.id.problem_not_exist).setOnClickListener(this);
//    mSimpleProblems.findViewById(R.id.problem_closed_repair).setOnClickListener(this);
//    mSimpleProblems.findViewById(R.id.problem_duplicated_place).setOnClickListener(this);
    mSimpleProblems.findViewById(R.id.problem_other).setOnClickListener(this);
    mAdvancedProblem = view.findViewById(R.id.ll__other_problem);
    mProblemInput = mAdvancedProblem.findViewById(R.id.input);
    refreshProblems();
  }

  private void refreshProblems()
  {
    UiUtils.showIf(mAdvancedMode, mAdvancedProblem, mSave);
    UiUtils.showIf(!mAdvancedMode, mSimpleProblems);
  }

  private void send(String text)
  {
    Editor.nativeCreateNote(text);
    getToolbarController().onUpClick();
  }

  private void sendNotExist()
  {
    Editor.nativePlaceDoesNotExist("");
    getToolbarController().onUpClick();
  }

  @Override
  public void onClick(View v)
  {
    final int id = v.getId();
    if (id == R.id.problem_not_exist)
      sendNotExist();
    else if (id == R.id.problem_other)
    {
      mAdvancedMode = true;
      refreshProblems();
    }
    else if (id == R.id.save)
    {
      String text = mProblemInput.getText().toString().trim();
      if (TextUtils.isEmpty(text))
        return;
      send(text);
    }
  }
}
