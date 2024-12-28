import Chart
import CoreApi

protocol ElevationProfilePresenterProtocol: UICollectionViewDataSource, UICollectionViewDelegateFlowLayout {
  func configure()

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
  private let trackInfo: TrackInfo
  private let profileData: ElevationProfileData?
  private let delegate: ElevationProfileViewControllerDelegate?

  private let cellSpacing: CGFloat = 8
  private let descriptionModels: [DescriptionsViewModel]
  private let chartData: ElevationProfileChartData?
  private let formatter: ElevationProfileFormatter

  init(view: ElevationProfileViewProtocol,
       trackInfo: TrackInfo,
       profileData: ElevationProfileData?,
       formatter: ElevationProfileFormatter = ElevationProfileFormatter(),
       delegate: ElevationProfileViewControllerDelegate?) {
    self.view = view
    self.trackInfo = trackInfo
    self.profileData = profileData
    self.delegate = delegate
    if let profileData {
      self.chartData = ElevationProfileChartData(profileData)
    } else {
      self.chartData = nil
    }
    self.formatter = formatter

    descriptionModels = [
      DescriptionsViewModel(title: L("elevation_profile_ascent"), value: trackInfo.ascent, imageName: "ic_em_ascent_24"),
      DescriptionsViewModel(title: L("elevation_profile_descent"), value: trackInfo.descent, imageName: "ic_em_descent_24"),
      DescriptionsViewModel(title: L("elevation_profile_max_elevation"), value: trackInfo.maxElevation, imageName: "ic_em_max_attitude_24"),
      DescriptionsViewModel(title: L("elevation_profile_min_elevation"), value: trackInfo.minElevation, imageName: "ic_em_min_attitude_24")
    ]
  }

  deinit {
    BookmarksManager.shared().resetElevationActivePointChanged()
    BookmarksManager.shared().resetElevationMyPositionChanged()
  }
}

extension ElevationProfilePresenter: ElevationProfilePresenterProtocol {
  func configure() {
    guard let profileData, let chartData else {
      view?.isChartViewHidden = true
      view?.isDifficultyHidden = true
      view?.isExtendedDifficultyLabelHidden = true
      view?.isBottomPanelHidden = true
      return
    }
    view?.isChartViewHidden = false

    if profileData.difficulty != .disabled {
      view?.isDifficultyHidden = false
      view?.setDifficulty(profileData.difficulty)
    } else {
      view?.isDifficultyHidden = true
    }

    view?.isBottomPanelHidden = profileData.difficulty == .disabled
    view?.isExtendedDifficultyLabelHidden = true

    let presentationData = ChartPresentationData(chartData, formatter: formatter)
    view?.setChartData(presentationData)
    view?.setActivePoint(profileData.activePoint)
    view?.setMyPosition(profileData.myPosition)

    BookmarksManager.shared().setElevationActivePointChanged(profileData.trackId) { [weak self] distance in
      self?.view?.setActivePoint(distance)
    }
    BookmarksManager.shared().setElevationMyPositionChanged(profileData.trackId) { [weak self] distance in
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
    let cell = collectionView.dequeueReusableCell(withReuseIdentifier: "ElevationProfileDescriptionCell", for: indexPath) as! ElevationProfileDescriptionCell
    let model = descriptionModels[indexPath.row]
    cell.configure(title: model.title, value: formatter.yAxisString(from: Double(model.value)), imageName: model.imageName)
    return cell
  }
}

// MARK: - UICollectionViewDelegateFlowLayout

extension ElevationProfilePresenter {
  func collectionView(_ collectionView: UICollectionView, layout collectionViewLayout: UICollectionViewLayout, sizeForItemAt indexPath: IndexPath) -> CGSize {
    let width = collectionView.width
    let cellHeight = collectionView.height
    let modelsCount = CGFloat(descriptionModels.count)
    let cellWidth = (width - cellSpacing * (modelsCount - 1)) / modelsCount
    return CGSize(width: cellWidth, height: cellHeight)
  }

  func collectionView(_ collectionView: UICollectionView, layout collectionViewLayout: UICollectionViewLayout, minimumInteritemSpacingForSectionAt section: Int) -> CGFloat {
    return cellSpacing
  }
}

fileprivate struct ElevationProfileChartData {

  struct Line: ChartLine {
    var values: [ChartValue]
    var name: String
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
    let l1 = Line(values: chartValues, name: "Altitude", color: lineColor, type: .line)
    let l2 = Line(values: chartValues, name: "Altitude", color: lineShadowColor, type: .lineArea)
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
