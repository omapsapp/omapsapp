import Chart

protocol ElevationProfileViewProtocol: AnyObject {
  var presenter: ElevationProfilePresenterProtocol?  { get set }

  var isChartViewHidden: Bool { get set }
  var isChartViewInfoHidden: Bool { get set }

  func setChartData(_ data: ChartPresentationData)
  func setActivePoint(_ distance: Double)
  func setMyPosition(_ distance: Double)
  func reloadDescription()
}

final class ElevationProfileViewController: UIViewController {

  private enum Constants {
    static let descriptionCollectionViewHeight: CGFloat = 52
    static let descriptionCollectionViewContentInsets = UIEdgeInsets(top: 20, left: 16, bottom: 4, right: 16)
    static let graphViewContainerInsets = UIEdgeInsets(top: -4, left: 0, bottom: 0, right: 0)
    static let chartViewInsets = UIEdgeInsets(top: 0, left: 16, bottom: 0, right: -16)
    static let chartViewVisibleHeight: CGFloat = 176
    static let chartViewHiddenHeight: CGFloat = .zero
  }
  
  var presenter: ElevationProfilePresenterProtocol?

  init() {
    super.init(nibName: nil, bundle: nil)
  }

  @available(*, unavailable)
  required init?(coder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }
  
  private var chartView = ChartView()
  private var graphViewContainer = UIView()
  private var descriptionCollectionView: UICollectionView = {
    let layout = UICollectionViewFlowLayout()
    layout.scrollDirection = .horizontal
    layout.minimumInteritemSpacing = 0
    return UICollectionView(frame: .zero, collectionViewLayout: layout)
  }()
  private var chartViewHeightConstraint: NSLayoutConstraint!


  // MARK: - Lifecycle

  override func viewDidLoad() {
    super.viewDidLoad()
    setupViews()
    layoutViews()
    presenter?.configure()
  }

  override func viewWillLayoutSubviews() {
    super.viewWillLayoutSubviews()
    descriptionCollectionView.reloadData()
  }

// MARK: - Private methods

  private func setupViews() {
    view.styleName = "Background"
    setupDescriptionCollectionView()
    setupChartView()
  }

  private func setupChartView() {
    graphViewContainer.translatesAutoresizingMaskIntoConstraints = false
    chartView.translatesAutoresizingMaskIntoConstraints = false
    chartView.onSelectedPointChanged = { [weak self] in
      self?.presenter?.onSelectedPointChanged($0)
    }
  }

  private func setupDescriptionCollectionView() {
    descriptionCollectionView.backgroundColor = .clear
    descriptionCollectionView.register(cell: ElevationProfileDescriptionCell.self)
    descriptionCollectionView.dataSource = presenter
    descriptionCollectionView.delegate = presenter
    descriptionCollectionView.isScrollEnabled = false
    descriptionCollectionView.contentInset = Constants.descriptionCollectionViewContentInsets
    descriptionCollectionView.translatesAutoresizingMaskIntoConstraints = false
  }

  private func layoutViews() {
    view.addSubview(descriptionCollectionView)
    graphViewContainer.addSubview(chartView)
    view.addSubview(graphViewContainer)

    chartViewHeightConstraint = chartView.heightAnchor.constraint(equalToConstant: Constants.chartViewVisibleHeight)
    NSLayoutConstraint.activate([
      descriptionCollectionView.topAnchor.constraint(equalTo: view.topAnchor),
      descriptionCollectionView.leadingAnchor.constraint(equalTo: view.leadingAnchor),
      descriptionCollectionView.trailingAnchor.constraint(equalTo: view.trailingAnchor),
      descriptionCollectionView.heightAnchor.constraint(equalToConstant: Constants.descriptionCollectionViewHeight),
      descriptionCollectionView.bottomAnchor.constraint(equalTo: graphViewContainer.topAnchor, constant: Constants.graphViewContainerInsets.top),
      graphViewContainer.leadingAnchor.constraint(equalTo: view.leadingAnchor),
      graphViewContainer.trailingAnchor.constraint(equalTo: view.trailingAnchor),
      graphViewContainer.bottomAnchor.constraint(equalTo: view.bottomAnchor),
      chartView.topAnchor.constraint(equalTo: graphViewContainer.topAnchor),
      chartView.leadingAnchor.constraint(equalTo: graphViewContainer.leadingAnchor, constant: Constants.chartViewInsets.left),
      chartView.trailingAnchor.constraint(equalTo: graphViewContainer.trailingAnchor, constant: Constants.chartViewInsets.right),
      chartView.bottomAnchor.constraint(equalTo: graphViewContainer.bottomAnchor),
      chartViewHeightConstraint,
    ])
  }

  private func getPreviewHeight() -> CGFloat {
    view.height - descriptionCollectionView.frame.minY
  }
}

// MARK: - ElevationProfileViewProtocol

extension ElevationProfileViewController: ElevationProfileViewProtocol {
  var isChartViewHidden: Bool {
    get { chartView.isHidden }
    set {
      chartView.isHidden = newValue
      graphViewContainer.isHidden = newValue
      chartViewHeightConstraint.constant = newValue ? Constants.chartViewHiddenHeight : Constants.chartViewVisibleHeight
    }
  }

  var isChartViewInfoHidden: Bool {
    get { chartView.isChartViewInfoHidden }
    set { chartView.isChartViewInfoHidden = newValue }
  }

  func setChartData(_ data: ChartPresentationData) {
    chartView.chartData = data
  }

  func setActivePoint(_ distance: Double) {
    chartView.setSelectedPoint(distance)
  }

  func setMyPosition(_ distance: Double) {
    chartView.myPosition = distance
  }

  func reloadDescription() {
    descriptionCollectionView.reloadData()
  }
}
