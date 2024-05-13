package app.organicmaps.editor;

import android.os.Bundle;

import androidx.annotation.NonNull;

import app.organicmaps.base.BaseMwmRecyclerFragment;
import app.organicmaps.sdk.editor.Editor;
import app.organicmaps.sdk.editor.data.Language;

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

  @NonNull
  @Override
  protected LanguagesAdapter createAdapter()
  {
    Bundle args = getArguments();
    Set<String> existingLanguages = new HashSet<>(args.getStringArrayList(EXISTING_LOCALIZED_NAMES));

    List<Language> languages = new ArrayList<>();
    for (Language lang : Editor.nativeGetSupportedLanguages())
    {
      if (!existingLanguages.contains(lang.code))
        languages.add(lang);
    }

    Collections.sort(languages, Comparator.comparing(lhs -> lhs.name));

    return new LanguagesAdapter(this, languages.toArray(new Language[languages.size()]));
  }

  protected void onLanguageSelected(Language language)
  {
    if (getParentFragment() instanceof Listener)
      ((Listener) getParentFragment()).onLanguageSelected(language);
  }
}
