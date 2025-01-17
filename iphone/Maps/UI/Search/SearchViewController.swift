import UIKit

enum SearchState {
  case recents
  case suggestions
  case resultsWithFilters
}

class SearchInteractor {
  func search(for text: String)
}

final class SearchViewController: UIViewController {

  // MARK: - Constants
  fileprivate enum Constants {
    static let grabberHeight: CGFloat = 5
    static let grabberWidth: CGFloat = 36
    static let grabberTopMargin: CGFloat = 5
    static let categoriesHeight: CGFloat = 100
    static let filtersHeight: CGFloat = 50
    static let keyboardAnimationDuration: CGFloat = 0.3
  }

  // MARK: - Properties
  private var currentState: SearchState = .recents {
    didSet {
      updateContentForCurrentState()
    }
  }
  private var keyboardHeight: CGFloat = 0
  private var categoriesBottomConstraint: NSLayoutConstraint?

  // MARK: - UI Elements
  private let searchBar = UISearchBar()
  private let containerView = UIView()
  private let grabberView = UIView()
  private let recentsTableView = UITableView()
  private let suggestionsTableView = UITableView()
  private let resultsTableView = UITableView()
  private let categoriesCollectionView: UICollectionView = {
    let layout = UICollectionViewFlowLayout()
    layout.scrollDirection = .horizontal
    return UICollectionView(frame: .zero, collectionViewLayout: layout)
  }()
  private let filtersCollectionView: UICollectionView = {
    let layout = UICollectionViewFlowLayout()
    layout.scrollDirection = .horizontal
    return UICollectionView(frame: .zero, collectionViewLayout: layout)
  }()

  // MARK: - Init
  init() {
    super.init(nibName: nil, bundle: nil)
  }

  @available(*, unavailable)
  required init?(coder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  deinit {
    NotificationCenter.default.removeObserver(self)
  }

  // MARK: - Lifecycle
  override func viewDidLoad() {
    super.viewDidLoad()
    setupViews()
    layoutViews()
    setupKeyboardObservers()
    updateContentForCurrentState()
  }

  override func viewWillDisappear(_ animated: Bool) {
    super.viewWillDisappear(animated)
    searchBar.resignFirstResponder()
  }


  // MARK: - Private methods
  private func setupViews() {
    setupGrabberView()
    setupSearchBar()
    setupContainerView()
    setupRecentsTableView()
    setupSuggestionsTableView()
    setupResultsTableView()
    setupCategoriesCollectionView()
    setupFiltersCollectionView()
  }

  private func setupGrabberView() {
    view.addSubview(grabberView)
    grabberView.setStyle(.background)
    grabberView.layer.setCorner(radius: Constants.grabberHeight / 2)
    iPadSpecific { [weak self] in
      self?.grabberView.isHidden = true
    }
  }

  private func setupSearchBar() {
    searchBar.placeholder = "Search"
    searchBar.delegate = self
    searchBar.showsCancelButton = true
    if #available(iOS 13.0, *) {
      searchBar.searchTextField.clearButtonMode = .always
      searchBar.searchTextField.becomeFirstResponder()
    }
    view.addSubview(searchBar)
  }

  private func setupContainerView() {
    containerView.backgroundColor = .clear
    view.addSubview(containerView)
  }

  private func setupRecentsTableView() {
    recentsTableView.register(UITableViewCell.self, forCellReuseIdentifier: "RecentsCell")
    recentsTableView.dataSource = self
  }

  private func setupSuggestionsTableView() {
    suggestionsTableView.register(UITableViewCell.self, forCellReuseIdentifier: "SuggestionsCell")
    suggestionsTableView.dataSource = self
  }

  private func setupResultsTableView() {
    resultsTableView.register(UITableViewCell.self, forCellReuseIdentifier: "ResultsCell")
    resultsTableView.dataSource = self
  }

  private func setupCategoriesCollectionView() {
    categoriesCollectionView.register(UICollectionViewCell.self, forCellWithReuseIdentifier: "CategoryCell")
    categoriesCollectionView.dataSource = self
  }

  private func setupFiltersCollectionView() {
    filtersCollectionView.register(UICollectionViewCell.self, forCellWithReuseIdentifier: "FilterCell")
    filtersCollectionView.dataSource = self
  }

  private func layoutViews() {
    grabberView.translatesAutoresizingMaskIntoConstraints = false
    searchBar.translatesAutoresizingMaskIntoConstraints = false
    containerView.translatesAutoresizingMaskIntoConstraints = false

    NSLayoutConstraint.activate([
      grabberView.topAnchor.constraint(equalTo: view.topAnchor, constant: Constants.grabberTopMargin),
      grabberView.centerXAnchor.constraint(equalTo: view.centerXAnchor),
      grabberView.widthAnchor.constraint(equalToConstant: Constants.grabberWidth),
      grabberView.heightAnchor.constraint(equalToConstant: Constants.grabberHeight),

      searchBar.topAnchor.constraint(equalTo: grabberView.bottomAnchor),
      searchBar.leadingAnchor.constraint(equalTo: view.leadingAnchor),
      searchBar.trailingAnchor.constraint(equalTo: view.trailingAnchor),

      containerView.topAnchor.constraint(equalTo: searchBar.bottomAnchor),
      containerView.leadingAnchor.constraint(equalTo: view.leadingAnchor),
      containerView.trailingAnchor.constraint(equalTo: view.trailingAnchor),
      containerView.bottomAnchor.constraint(equalTo: view.bottomAnchor)
    ])
  }

  // MARK: - Content Updates
  private func updateContentForCurrentState() {
    containerView.subviews.forEach { $0.removeFromSuperview() }

    switch currentState {
    case .recents:
      setupRecentsView()
    case .suggestions:
      setupSuggestionsView()
    case .resultsWithFilters:
      setupFiltersView()
    }
  }

  private func setupRecentsView() {
    containerView.addSubview(recentsTableView)
    containerView.addSubview(categoriesCollectionView)

    recentsTableView.translatesAutoresizingMaskIntoConstraints = false
    categoriesCollectionView.translatesAutoresizingMaskIntoConstraints = false

    categoriesBottomConstraint = categoriesCollectionView.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor)
    NSLayoutConstraint.activate([
      recentsTableView.topAnchor.constraint(equalTo: containerView.topAnchor),
      recentsTableView.leadingAnchor.constraint(equalTo: containerView.leadingAnchor),
      recentsTableView.trailingAnchor.constraint(equalTo: containerView.trailingAnchor),

      categoriesCollectionView.topAnchor.constraint(equalTo: recentsTableView.bottomAnchor),
      categoriesCollectionView.leadingAnchor.constraint(equalTo: containerView.leadingAnchor),
      categoriesCollectionView.trailingAnchor.constraint(equalTo: containerView.trailingAnchor),
      categoriesCollectionView.heightAnchor.constraint(equalToConstant: Constants.categoriesHeight),
      categoriesBottomConstraint!
    ])
  }

  private func setupSuggestionsView() {
    containerView.addSubview(suggestionsTableView)
    containerView.addSubview(resultsTableView)

    suggestionsTableView.translatesAutoresizingMaskIntoConstraints = false
    resultsTableView.translatesAutoresizingMaskIntoConstraints = false

    NSLayoutConstraint.activate([
      suggestionsTableView.topAnchor.constraint(equalTo: containerView.topAnchor),
      suggestionsTableView.leadingAnchor.constraint(equalTo: containerView.leadingAnchor),
      suggestionsTableView.trailingAnchor.constraint(equalTo: containerView.trailingAnchor),

      resultsTableView.topAnchor.constraint(equalTo: suggestionsTableView.bottomAnchor),
      resultsTableView.leadingAnchor.constraint(equalTo: containerView.leadingAnchor),
      resultsTableView.trailingAnchor.constraint(equalTo: containerView.trailingAnchor),
      resultsTableView.bottomAnchor.constraint(equalTo: containerView.bottomAnchor)
    ])
  }

  private func setupFiltersView() {
    containerView.addSubview(filtersCollectionView)
    containerView.addSubview(resultsTableView)

    filtersCollectionView.translatesAutoresizingMaskIntoConstraints = false
    resultsTableView.translatesAutoresizingMaskIntoConstraints = false

    NSLayoutConstraint.activate([
      filtersCollectionView.topAnchor.constraint(equalTo: containerView.topAnchor),
      filtersCollectionView.leadingAnchor.constraint(equalTo: containerView.leadingAnchor),
      filtersCollectionView.trailingAnchor.constraint(equalTo: containerView.trailingAnchor),
      filtersCollectionView.heightAnchor.constraint(equalToConstant: Constants.filtersHeight),

      resultsTableView.topAnchor.constraint(equalTo: filtersCollectionView.bottomAnchor),
      resultsTableView.leadingAnchor.constraint(equalTo: containerView.leadingAnchor),
      resultsTableView.trailingAnchor.constraint(equalTo: containerView.trailingAnchor),
      resultsTableView.bottomAnchor.constraint(equalTo: containerView.bottomAnchor)
    ])
  }

  // MARK: - Keyboard Observers
  private func setupKeyboardObservers() {
    NotificationCenter.default.addObserver(self,
                                           selector: #selector(keyboardWillShow(_:)),
                                           name: UIResponder.keyboardWillShowNotification,
                                           object: nil)
    NotificationCenter.default.addObserver(self,
                                           selector: #selector(keyboardWillHide(_:)),
                                           name: UIResponder.keyboardWillHideNotification,
                                           object: nil)
  }

  @objc private func keyboardWillShow(_ notification: Notification) {
    if let keyboardFrame = notification.userInfo?[UIResponder.keyboardFrameEndUserInfoKey] as? CGRect {
      keyboardHeight = keyboardFrame.height
      updateCategoriesBottomConstraint()
    }
  }

  @objc private func keyboardWillHide(_ notification: Notification) {
    keyboardHeight = 0
    updateCategoriesBottomConstraint()
  }

  private func updateCategoriesBottomConstraint() {
    categoriesBottomConstraint?.constant = -keyboardHeight
    UIView.animate(withDuration: Constants.keyboardAnimationDuration) {
      self.view.layoutIfNeeded()
    }
  }
}

// MARK: - UISearchBarDelegate
extension SearchViewController: UISearchBarDelegate {
  func searchBarTextDidBeginEditing(_ searchBar: UISearchBar) {
    currentState = .suggestions
  }

  func searchBarSearchButtonClicked(_ searchBar: UISearchBar) {
    currentState = .resultsWithFilters
  }

  func searchBarCancelButtonClicked(_ searchBar: UISearchBar) {
    dismiss(animated: true, completion: nil)
  }
}

// MARK: - UITableViewDataSource
extension SearchViewController: UITableViewDataSource {
  func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
    return 10
  }

  func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
    if tableView == recentsTableView {
      let cell = tableView.dequeueReusableCell(withIdentifier: "RecentsCell", for: indexPath)
      cell.textLabel?.text = "Recent \(indexPath.row)"
      return cell
    } else if tableView == suggestionsTableView {
      let cell = tableView.dequeueReusableCell(withIdentifier: "SuggestionsCell", for: indexPath)
      cell.textLabel?.text = "Suggestion \(indexPath.row)"
      return cell
    } else {
      let cell = tableView.dequeueReusableCell(withIdentifier: "ResultsCell", for: indexPath)
      cell.textLabel?.text = "Result \(indexPath.row)"
      return cell
    }
  }
}

// MARK: - UICollectionViewDataSource
extension SearchViewController: UICollectionViewDataSource {
  func collectionView(_ collectionView: UICollectionView, numberOfItemsInSection section: Int) -> Int {
    return 10
  }

  func collectionView(_ collectionView: UICollectionView, cellForItemAt indexPath: IndexPath) -> UICollectionViewCell {
    let cell = collectionView.dequeueReusableCell(withReuseIdentifier: collectionView == categoriesCollectionView ? "CategoryCell" : "FilterCell", for: indexPath)
    return cell
  }
}
