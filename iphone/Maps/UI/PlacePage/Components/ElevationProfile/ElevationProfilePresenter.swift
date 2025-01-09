import Chart
import CoreApi

protocol ElevationProfilePresenterProtocol: UICollectionViewDataSource, UICollectionViewDelegateFlowLayout {
  func configure()
  func update(trackInfo: TrackInfo, profileData: ElevationProfileData?)

  func onDifficultyButtonPressed()
  func onSelectedPointChanged(_ point: CGFloat)
}

protocol ElevationProfileViewControllerDelegate: AnyObject {
  func openDifficultyPopup()
  func updateMapPoint(_ point: CLLocationCoordinate2D, distance: Double)
}

fileprivate struct DescriptionsViewModel {
  let title: String
  let value: UInt
  let imageName: String
}

final class ElevationProfilePresenter: NSObject {
  private weak var view: ElevationProfileViewProtocol?
  private var trackInfo: TrackInfo
  private var profileData: ElevationProfileData?
  private let delegate: ElevationProfileViewControllerDelegate?
  private let bookmarkManager: BookmarksManager = .shared()

  private let cellSpacing: CGFloat = 8
  private var descriptionModels: [DescriptionsViewModel]
  private var chartData: ElevationProfileChartData?
  private let formatter: ElevationProfileFormatter

  init(view: ElevationProfileViewProtocol,
       trackInfo: TrackInfo,
       profileData: ElevationProfileData?,
       formatter: ElevationProfileFormatter = ElevationProfileFormatter(),
       delegate: ElevationProfileViewControllerDelegate?) {
    self.view = view
    self.delegate = delegate
    self.formatter = formatter
    self.trackInfo = trackInfo
    self.profileData = profileData
    if let profileData {
      self.chartData = ElevationProfileChartData(profileData)
    }
    self.descriptionModels = Self.descriptionModels(for: trackInfo)
  }

  private static func descriptionModels(for trackInfo: TrackInfo) -> [DescriptionsViewModel] {
    [
      DescriptionsViewModel(title: L("elevation_profile_ascent"), value: trackInfo.ascent, imageName: "ic_em_ascent_24"),
      DescriptionsViewModel(title: L("elevation_profile_descent"), value: trackInfo.descent, imageName: "ic_em_descent_24"),
      DescriptionsViewModel(title: L("elevation_profile_max_elevation"), value: trackInfo.maxElevation, imageName: "ic_em_max_attitude_24"),
      DescriptionsViewModel(title: L("elevation_profile_min_elevation"), value: trackInfo.minElevation, imageName: "ic_em_min_attitude_24")
    ]
  }

  deinit {
    bookmarkManager.resetElevationActivePointChanged()
    bookmarkManager.resetElevationMyPositionChanged()
  }
}

extension ElevationProfilePresenter: ElevationProfilePresenterProtocol {
  func update(trackInfo: TrackInfo, profileData: ElevationProfileData?) {
    self.profileData = profileData
    if let profileData {
      self.chartData = ElevationProfileChartData(profileData)
    } else {
      self.chartData = nil
    }
    descriptionModels = Self.descriptionModels(for: trackInfo)
    configure()
  }

  func configure() {
    let kMinPointsToDraw = 3
    guard let profileData, let chartData, chartData.points.count > kMinPointsToDraw else {
      view?.isChartViewHidden = true
      return
    }
    view?.isChartViewHidden = false
    view?.setChartData(ChartPresentationData(chartData, formatter: formatter))
    view?.reloadDescription()

    guard !profileData.isTrackRecording else {
      view?.isChartViewInfoHidden = true
      return
    }

    view?.setActivePoint(profileData.activePoint)
    view?.setMyPosition(profileData.myPosition)
    bookmarkManager.setElevationActivePointChanged(profileData.trackId) { [weak self] distance in
      self?.view?.setActivePoint(distance)
    }
    bookmarkManager.setElevationMyPositionChanged(profileData.trackId) { [weak self] distance in
      self?.view?.setMyPosition(distance)
    }
  }

  func onDifficultyButtonPressed() {
    delegate?.openDifficultyPopup()
  }

  func onSelectedPointChanged(_ point: CGFloat) {
    guard let chartData else { return }
    let distance: Double = floor(point) / CGFloat(chartData.points.count) * chartData.maxDistance
    let point = chartData.points.first { $0.distance >= distance } ?? chartData.points[0]
    delegate?.updateMapPoint(point.coordinates, distance: point.distance)
  }
}

// MARK: - UICollectionDataSource

extension ElevationProfilePresenter {
  func collectionView(_ collectionView: UICollectionView, numberOfItemsInSection section: Int) -> Int {
    return descriptionModels.count
  }

  func collectionView(_ collectionView: UICollectionView, cellForItemAt indexPath: IndexPath) -> UICollectionViewCell {
    let cell = collectionView.dequeueReusableCell(cell: ElevationProfileDescriptionCell.self, indexPath: indexPath)
    let model = descriptionModels[indexPath.row]
    cell.configure(subtitle: model.title, value: formatter.yAxisString(from: Double(model.value)), imageName: model.imageName)
    return cell
  }
}

// MARK: - UICollectionViewDelegateFlowLayout

extension ElevationProfilePresenter {
  func collectionView(_ collectionView: UICollectionView, layout collectionViewLayout: UICollectionViewLayout, sizeForItemAt indexPath: IndexPath) -> CGSize {
    let width = collectionView.width
    let cellHeight = collectionView.height
    let modelsCount = CGFloat(descriptionModels.count)
    let cellWidth = (width - cellSpacing * (modelsCount - 1) - collectionView.contentInset.right) / modelsCount
    return CGSize(width: cellWidth, height: cellHeight)
  }

  func collectionView(_ collectionView: UICollectionView, layout collectionViewLayout: UICollectionViewLayout, minimumInteritemSpacingForSectionAt section: Int) -> CGFloat {
    return cellSpacing
  }
}

fileprivate struct ElevationProfileChartData {

  struct Line: ChartLine {
    var values: [ChartValue]
    var color: UIColor
    var type: ChartLineType
  }

  fileprivate let chartValues: [ChartValue]
  fileprivate let chartLines: [Line]
  fileprivate let distances: [Double]
  fileprivate let maxDistance: Double
  fileprivate let points: [ElevationHeightPoint]

  init(_ elevationData: ElevationProfileData) {
    self.points = elevationData.points
    self.chartValues = points.map { ChartValue(xValues: $0.distance, y: $0.altitude) }
    self.distances = points.map { $0.distance }
    self.maxDistance = distances.last ?? 0
    let lineColor = StyleManager.shared.theme?.colors.chartLine ?? .blue
    let lineShadowColor = StyleManager.shared.theme?.colors.chartShadow ?? .lightGray
    let l1 = Line(values: chartValues, color: lineColor, type: .line)
    let l2 = Line(values: chartValues, color: lineShadowColor, type: .lineArea)
    chartLines = [l1, l2]
  }

  private static func altBetweenPoints(_ p1: ElevationHeightPoint,
                                       _ p2: ElevationHeightPoint,
                                       at distance: Double) -> Double {
    assert(distance > p1.distance && distance < p2.distance, "distance must be between points")
    let d = (distance - p1.distance) / (p2.distance - p1.distance)
    return p1.altitude + round(Double(p2.altitude - p1.altitude) * d)
  }
}

extension ElevationProfileChartData: ChartData {
  public var xAxisValues: [Double] { distances }
  public var lines: [ChartLine] { chartLines }
  public var type: ChartType { .regular }
}
