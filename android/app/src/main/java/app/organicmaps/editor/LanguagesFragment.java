package app.organicmaps.editor;

import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;

import app.organicmaps.base.BaseMwmRecyclerFragment;
import app.organicmaps.editor.data.Language;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashSet;
import java.util.List;
import java.util.Set;


public class LanguagesFragment extends BaseMwmRecyclerFragment<LanguagesAdapter>
{
  final static String EXISTING_LOCALIZED_NAMES = "ExistingLocalizedNames";

  public interface Listener
  {
    void onLanguageSelected(Language language);
  }

  private Listener mListener;

  @NonNull
  @Override
  protected LanguagesAdapter createAdapter()
  {
    Bundle args = getArguments();
    Set<String> existingLanguages = args != null
            ? new HashSet<>(args.getStringArrayList(EXISTING_LOCALIZED_NAMES))
            : new HashSet<>();

    List<Language> languages = new ArrayList<>();
    for (Language lang : Editor.nativeGetSupportedLanguages(false))
    {
      if (existingLanguages.contains(lang.code))
        continue;

      languages.add(lang);
    }

    Collections.sort(languages, Comparator.comparing(lhs -> lhs.name));

    return new LanguagesAdapter(this, languages.toArray(new Language[languages.size()]));
  }

  public void setListener(Listener listener)
  {
    this.mListener = listener;
  }

  protected void onLanguageSelected(Language language)
  {
    Fragment parent = getParentFragment();
    if (parent instanceof Listener)
      ((Listener) parent).onLanguageSelected(language);
    if (mListener != null)
      mListener.onLanguageSelected(language);
  }
}
