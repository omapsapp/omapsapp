import Chart

protocol ElevationProfileViewProtocol: AnyObject {
  var presenter: ElevationProfilePresenterProtocol?  { get set }

  var isChartViewHidden: Bool { get set }
  var isExtendedDifficultyLabelHidden: Bool { get set }
  var isDifficultyHidden: Bool { get set }
  var isBottomPanelHidden: Bool { get set }

  func setExtendedDifficultyGrade(_ value: String)
  func setDifficulty(_ value: ElevationDifficulty)
  func setChartData(_ data: ChartPresentationData)
  func setActivePoint(_ distance: Double)
  func setMyPosition(_ distance: Double)
}

class ElevationProfileViewController: UIViewController {

  private enum Constants {
    static let chartViewVisibleHeight: CGFloat = 176
    static let chartViewHiddenHeight: CGFloat = 20
    static let difficultyVisibleHeight: CGFloat = 60
    static let difficultyHiddenHeight: CGFloat = 20
  }

  var presenter: ElevationProfilePresenterProtocol?
  
  @IBOutlet private weak var chartView: ChartView!
  @IBOutlet private weak var graphViewContainer: UIView!
  @IBOutlet private weak var descriptionCollectionView: UICollectionView!
  @IBOutlet private weak var difficultyView: DifficultyView!
  @IBOutlet private weak var difficultyTitle: UILabel!
  @IBOutlet private weak var extendedDifficultyGradeLabel: UILabel!
  @IBOutlet private weak var extendedGradeButton: UIButton!
  @IBOutlet private weak var chartHeightConstraint: NSLayoutConstraint!
  @IBOutlet private weak var difficultyConstraint: NSLayoutConstraint!

  private var difficultyHidden: Bool = false
  private var bottomPanelHidden: Bool = false

  override func viewDidLoad() {
    super.viewDidLoad()
    descriptionCollectionView.dataSource = presenter
    descriptionCollectionView.delegate = presenter
    presenter?.configure()
    chartView.onSelectedPointChanged = { [weak self] in
      self?.presenter?.onSelectedPointChanged($0)
    }
  }

  override func viewWillLayoutSubviews() {
    super.viewWillLayoutSubviews()
    descriptionCollectionView.reloadData()
  }

  @IBAction func onExtendedDifficultyButtonPressed(_ sender: Any) {
    presenter?.onDifficultyButtonPressed()
  }

  func getPreviewHeight() -> CGFloat {
    return view.height - descriptionCollectionView.frame.minY
  }
}

extension ElevationProfileViewController: ElevationProfileViewProtocol {
  var isChartViewHidden: Bool {
    get { return chartView.isHidden }
    set {
      chartView.isHidden = newValue
      graphViewContainer.isHidden = newValue
      chartHeightConstraint.constant = newValue ? Constants.chartViewHiddenHeight : Constants.chartViewVisibleHeight
    }
  }

  var isExtendedDifficultyLabelHidden: Bool {
    get { return extendedDifficultyGradeLabel.isHidden }
    set {
      extendedDifficultyGradeLabel.isHidden = newValue
      extendedGradeButton.isHidden = newValue
    }
  }

  var isDifficultyHidden: Bool {
    get { difficultyHidden }
    set {
      difficultyHidden = newValue
      difficultyTitle.isHidden = newValue
      difficultyView.isHidden = newValue
    }
  }

  var isBottomPanelHidden: Bool {
    get { bottomPanelHidden }
    set {
      bottomPanelHidden = newValue
      if newValue == true {
        isExtendedDifficultyLabelHidden = true
        isDifficultyHidden = true
      }
      difficultyConstraint.constant = newValue ? Constants.difficultyHiddenHeight : Constants.difficultyVisibleHeight
    }
  }
  
  func setExtendedDifficultyGrade(_ value: String) {
    extendedDifficultyGradeLabel.text = value
  }

  func setDifficulty(_ value: ElevationDifficulty) {
    difficultyView.difficulty = value
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
}
