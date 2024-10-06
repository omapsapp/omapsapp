protocol BottomMenuViewProtocol: AnyObject {
  var presenter: BottomMenuPresenterProtocol?  { get set }
}

class BottomMenuViewController: MWMViewController {
  var presenter: BottomMenuPresenterProtocol?
  private let transitioningManager = BottomMenuTransitioningManager()
  
  @IBOutlet var tableView: UITableView!
  @IBOutlet var heightConstraint: NSLayoutConstraint!
  @IBOutlet var bottomConstraint: NSLayoutConstraint!

  lazy var chromeView: UIView = {
    let view = UIView()
    view.styleName = "PresentationBackground"
    return view
  }()
  
  weak var containerView: UIView! {
    didSet {
      containerView.insertSubview(chromeView, at: 0)
    }
  }
  
  override func viewDidLoad() {
    super.viewDidLoad()
    
    tableView.layer.setCorner(radius: 8, corners: [.layerMinXMinYCorner, .layerMaxXMinYCorner])
    tableView.sectionFooterHeight = 0
    
    tableView.dataSource = presenter
    tableView.delegate = presenter
    tableView.registerNib(cell: BottomMenuItemCell.self)
    tableView.registerNib(cell: BottomMenuLayersCell.self)
  }

  override func viewDidAppear(_ animated: Bool) {
    super.viewDidAppear(animated)
    if let cellToHighlight = presenter?.cellToHighlightIndexPath() {
      tableView.cellForRow(at: cellToHighlight)?.highlight()
    }
  }

  override func viewDidLayoutSubviews() {
    super.viewDidLayoutSubviews()
    tableView.layoutIfNeeded()
    heightConstraint.constant = min(tableView.contentSize.height, view.height)
    tableView.isScrollEnabled = tableView.contentSize.height > heightConstraint.constant;
  }
  
  @IBAction func onClosePressed(_ sender: Any) {
    presenter?.onClosePressed()
  }

  @IBAction func onPan(_ sender: UIPanGestureRecognizer) {
    let yOffset = sender.translation(in: view.superview).y
    let yVelocity = sender.velocity(in: view.superview).y
    sender.setTranslation(CGPoint.zero, in: view.superview)
    bottomConstraint.constant = min(bottomConstraint.constant - yOffset, 0);

    let alpha = 1.0 - abs(bottomConstraint.constant / tableView.height)
    self.chromeView.alpha = alpha

    let state = sender.state
    if state == .ended || state == .cancelled {
      if yVelocity > 0 || (yVelocity == 0 && alpha < 0.8) {
        presenter?.onClosePressed()
      } else {
        let duration = min(kDefaultAnimationDuration, TimeInterval(self.bottomConstraint.constant / yVelocity))
        self.view.layoutIfNeeded()
        UIView.animate(withDuration: duration) {
          self.chromeView.alpha = 1
          self.bottomConstraint.constant = 0
          self.view.layoutIfNeeded()
        }
      }
    }
  }

  override var transitioningDelegate: UIViewControllerTransitioningDelegate? {
    get { return transitioningManager }
    set { }
  }
  
  override var modalPresentationStyle: UIModalPresentationStyle {
    get { return .custom }
    set { }
  }
}

extension BottomMenuViewController: BottomMenuViewProtocol {
  
}
