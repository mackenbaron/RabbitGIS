#include "Mainwindow.h"
#include "ui_Mainwindow.h"
#include <vector>
#include <QFileDialog>
#include <QDebug>
#include <QSettings>
#include <QMessageBox>
#include "Global/GlobalDir.h"
#include "Global/Log.h"
#include "GpxModel/gpx_model.h"
#include "Global/Global.h"
#include "Common/Tool.h"
#include "Widgets/DlgAbout/DlgAbout.h"

#include <osgEarthAnnotation/FeatureNode>
#include <osgEarthFeatures/Feature>
#include <osgEarthSymbology/Style>
#include <osgEarthUtil/EarthManipulator>
#include <osgEarthAnnotation/PlaceNode>
#include <osgEarthUtil/LatLongFormatter>
#include <osgDB/WriteFile>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    m_ActionGroupTranslator(this),
    m_ActionGroupStyle(this),
    m_ActionGroupMap(this),
    m_pMeasureTool(NULL),
    ui(new Ui::MainWindow)
{
    int nRet = 0;
    ui->setupUi(this);
    LoadTranslate();
    LoadStyle();
    InitMenuStyles();
    InitMenuTranslate();
    this->statusBar()->setVisible(CGlobal::Instance()->GetStatusbarVisable());
    ui->actionStatusBar_S->setChecked(CGlobal::Instance()->GetStatusbarVisable());
    InitToolbar();
    
    m_Root = new osg::Group();
    osgViewer::Viewer* viewer = (osgViewer::Viewer*)m_MapViewer.getViewer();
    viewer->setSceneData(m_Root);
    this->setCentralWidget(&m_MapViewer);
    m_MapViewer.setCursor(Qt::OpenHandCursor);

    nRet = LoadProject(CGlobalDir::Instance()->GetDirData()
                   + QDir::separator()
                   + "Map_"
                   + (CGlobal::Instance()->GetLanguage() == "Default" 
                        ? QLocale::system().name()
                        : CGlobal::Instance()->GetLanguage())
                   + ".earth");
    if(nRet)
        nRet = LoadProject(CGlobalDir::Instance()->GetApplicationEarthFile());
    if(nRet)
        statusBar()->showMessage(tr("Open project fail"));
    
    bool check = connect(ui->menuMap_A, SIGNAL(aboutToShow()),
            SLOT(slotMenuMapShow()));
    Q_ASSERT(check);
}

MainWindow::~MainWindow()
{
#ifndef MOBILE
    //Save windows position  
    QSettings conf(CGlobalDir::Instance()->GetApplicationConfigureFile(),
                   QSettings::IniFormat);
    conf.setValue("UI/MainWindow/top", this->y());
    conf.setValue("UI/MainWindow/left", this->x());
    conf.setValue("UI/MainWindow/width", this->width());
    conf.setValue("UI/MainWindow/height", this->height());
#endif

    ui->menuMap_A->disconnect();
    ClearMenuStyles();
    ClearTranslate();
    ClearMenuTranslate();
    ClearMenuMap();
    delete ui;
}

int MainWindow::LoadProject(QString szFile)
{
    int nRet = 0;
    this->statusBar()->showMessage(tr("Loading project ...... "));
    m_MapViewer.setCursor(Qt::BusyCursor);
    do {
        osg::Node* mapNode = osgDB::readNodeFile(
                    szFile.toStdString());
        if(!mapNode)
        {
            LOG_MODEL_ERROR("MainWindow", "Open node file fail: %s",
                            szFile.toStdString().c_str());
            this->statusBar()->showMessage(tr("Load project fail:%1").arg(szFile));
            nRet = -1;
            break;
        }

        osgViewer::Viewer* viewer = (osgViewer::Viewer*)m_MapViewer.getViewer();

        // Clean
        viewer->removeEventHandler(m_MouseCoordsTool);
        m_Root->removeChild(m_MapNode);

        m_MapNode = osgEarth::MapNode::get(mapNode);

        // Set view port
        const osgEarth::SpatialReference* geoSRS =
                m_MapNode->getMapSRS()->getGeographicSRS();
        osgEarth::Util::EarthManipulator* em =
              (osgEarth::Util::EarthManipulator*)viewer->getCameraManipulator();
        if(!em)
        {
            LOG_MODEL_ERROR("MainWindow", "getCameraManipulator fail");
            nRet = -2;
            break;
        }
        em->setViewpoint(osgEarth::Viewpoint("China", 105, 35, 0, 0, -90,
                            geoSRS->getEllipsoid()->getRadiusEquator() * 2), 3); //3s, To China

        // Create display mouse coordinate canvas
        osgEarth::Util::Controls::ControlCanvas* pCanvas =
                osgEarth::Util::Controls::ControlCanvas::getOrCreate(viewer);
        if(m_MouseCanvasHBox)
        {
            pCanvas->removeControl(m_MouseCanvasHBox);
            m_MouseCanvasHBox.release();
        }

        m_MouseCanvasHBox = new osgEarth::Util::Controls::HBox();
        m_MouseCanvasHBox->setBackColor(0, 0, 0, 0.5);
        m_MouseCanvasHBox->setMargin(10);
        m_MouseCanvasHBox->setChildSpacing(80);
        m_MouseCanvasHBox->setVertAlign(osgEarth::Util::Controls::Control::ALIGN_BOTTOM);
        m_MouseCanvasHBox->setHorizAlign(osgEarth::Util::Controls::Control::ALIGN_CENTER);

        osgEarth::Util::Controls::LabelControl* pLabel =
                new osgEarth::Util::Controls::LabelControl();
        pLabel->setEncoding(osgText::String::ENCODING_UTF8);
        pLabel->setText(tr("Coordinate:").toUtf8().data());
        m_MouseCanvasHBox->addControl(pLabel);
        osgEarth::Util::Controls::LabelControl* mouseLabel =
                new osgEarth::Util::Controls::LabelControl();
        m_MouseCanvasHBox->addControl(mouseLabel);
        m_MouseCoordsTool = new osgEarth::Util::MouseCoordsTool(m_MapNode,
                                                                mouseLabel/*,
                                        new osgEarth::Util::LatLongFormatter(
                           osgEarth::Util::LatLongFormatter::FORMAT_DEFAULT*/);
        viewer->addEventHandler(m_MouseCoordsTool);
        pCanvas->addControl(m_MouseCanvasHBox.get());

        m_Root->addChild(m_MapNode);
        
    } while(0);
    this->statusBar()->showMessage(tr("Ready"));
    m_MapViewer.setCursor(Qt::OpenHandCursor);
    return nRet;
}

void MainWindow::on_actionOpen_Project_triggered()
{
    this->statusBar()->showMessage(tr("Open project file ......"));
    QString szFile = QFileDialog::getOpenFileName(this, tr("Open project file"), 
                             QString(), tr("Project file(*.earth);; All(*.*)"));
    if(!szFile.isEmpty())
        LoadProject(szFile);

    this->statusBar()->showMessage(tr("Ready"));
    return;
}

void MainWindow::on_actionSava_project_S_triggered()
{
    this->statusBar()->showMessage(tr("Save project to file ......"));
    QString szFile = QFileDialog::getSaveFileName(this, tr("Open project file"), 
                             QString(), tr("Project file(*.earth);; All(*.*)"));
    if(!szFile.isEmpty())
    {
        if(!osgDB::writeNodeFile(*m_Root, szFile.toStdString()))
        {
            LOG_MODEL_ERROR("MainWindow", "writeNodeFile fail:%s",
                            szFile.toStdString().c_str());
        }
    }

    this->statusBar()->showMessage(tr("Ready"));
    return;
    
}

void MainWindow::on_actionOpen_track_T_triggered()
{
    this->statusBar()->showMessage(tr("Open track file ......"));
    do{
        QString szFile = QFileDialog::getOpenFileName(this,
                                                      tr("Open track file"),
                                                                  QString(),
              tr("Track file(*.gpx);; nmea file(*.txt *.nmea);; All(*.*)"));
        if(szFile.isEmpty())
            break;

        m_MapViewer.setCursor(Qt::BusyCursor);
        GPX_model gpx("RabbitGIS");
        if(GPX_model::GPXM_OK != gpx.load(szFile.toStdString()))
        {
            LOG_MODEL_ERROR("MainWindow", "Open track file fail:%s",
                            szFile.toStdString());
            break;
        }

        osg::ref_ptr<osg::Group> track = new osg::Group();
        // Add track path
        osgEarth::Symbology::LineString* path =
                new osgEarth::Symbology::LineString();
        std::vector<GPX_trkType>::iterator it;
        for(it = gpx.trk.begin(); it != gpx.trk.end(); it++)
        {
            std::vector<GPX_trksegType>::iterator itSeg;
            for(itSeg = it->trkseg.begin(); itSeg != it->trkseg.end(); itSeg++)
            {
                std::vector<GPX_wptType>::iterator itWpt;
                for(itWpt = itSeg->trkpt.begin(); itWpt != itSeg->trkpt.end();
                    itWpt++)
                {
                    path->push_back(itWpt->longitude, itWpt->latitude); //, itWpt->geoidheight);
                }
            }
        }

        const osgEarth::SpatialReference* geoSRS =
                m_MapNode->getMapSRS()->getGeographicSRS();

        osgEarth::Annotation::Features::Feature* pathFeature =
                new osgEarth::Annotation::Features::Feature(path, geoSRS);
        pathFeature->geoInterp() = osgEarth::GEOINTERP_GREAT_CIRCLE;

        osgEarth::Style pathStyle;
        osgEarth::Symbology::LineSymbol* ls =
                pathStyle.getOrCreate<osgEarth::Symbology::LineSymbol>();
        ls->stroke()->color() = osgEarth::Color::Yellow;
        ls->stroke()->width() = 4.0f;
        ls->stroke()->widthUnits() = osgEarth::Units::PIXELS;
        ls->tessellation() = 150;

        /*pathStyle.getOrCreate<osgEarth::Symbology::PointSymbol>()->size() = 5;
       pathStyle.getOrCreate<osgEarth::Symbology::PointSymbol>()->fill()->color()
            = osgEarth::Color::Green;*/
        pathStyle.getOrCreate<osgEarth::Symbology::AltitudeSymbol>()->technique()
                =  osgEarth::AltitudeSymbol::TECHNIQUE_GPU;

        osgEarth::Annotation::FeatureNode* pathNode =
                new osgEarth::Annotation::FeatureNode(m_MapNode, pathFeature,
                                                      pathStyle);

        // Add start and end labels
        osg::Group* labelGroup = new osg::Group();
        track->addChild(labelGroup);
        osg::Vec3d start = path->at(0);
        osg::Vec3d end = path->at(path->size() - 1);
        osgEarth::Style pm;
        QString szStartIcon = CGlobalDir::Instance()->GetDirImage()
                + QDir::separator()
                + "Start32.png";
        pm.getOrCreate<osgEarth::IconSymbol>()->url()->setLiteral(
                    szStartIcon.toStdString());
        pm.getOrCreate<osgEarth::IconSymbol>()->declutter() = true;
        pm.getOrCreate<osgEarth::TextSymbol>()->halo() = osgEarth::Color("#5f5f5f");
        pm.getOrCreate<osgEarth::TextSymbol>()->encoding() =
                osgEarth::TextSymbol::ENCODING_UTF8;
        labelGroup->addChild(new osgEarth::Annotation::PlaceNode(m_MapNode,
                               osgEarth::GeoPoint(geoSRS, start.x(), start.y()),
                                              tr("Start").toUtf8().data(), pm));
        QString szEndIcon = CGlobalDir::Instance()->GetDirImage()
                + QDir::separator()
                + "End32.png";
        pm.getOrCreate<osgEarth::IconSymbol>()->url()->setLiteral(
                    szEndIcon.toStdString());
        labelGroup->addChild(new osgEarth::Annotation::PlaceNode(m_MapNode,
                                   osgEarth::GeoPoint(geoSRS, end.x(), end.y()),
                                                tr("End").toUtf8().data(), pm));
        track->addChild(pathNode);
        osg::ref_ptr<osgEarth::ModelLayer> layer 
                = new osgEarth::ModelLayer(tr("Track").toStdString(), track);
        m_MapNode->getMap()->addModelLayer(layer);
        //m_MapNode->addChild(track);

        // Set view port
        osgViewer::Viewer* viewer = (osgViewer::Viewer*)m_MapViewer.getViewer();
        osgEarth::Util::EarthManipulator* em =
              (osgEarth::Util::EarthManipulator*)viewer->getCameraManipulator();
        if(!em)
        {
            LOG_MODEL_ERROR("MainWindow", "getCameraManipulator fail");
            break;
        }
        double range = path->getBounds().width() > path->getBounds().height()
                ? path->getBounds().width()
                : path->getBounds().height();
        em->setViewpoint(osgEarth::Viewpoint("track",
                                             path->getBounds().center2d().x(),
                                             path->getBounds().center2d().y(),
                                             0, 0, -90,
                  range + geoSRS->getEllipsoid()->getRadiusEquator() * 0.2), 3);
    }while(0);
    this->statusBar()->showMessage(tr("Ready"));
    m_MapViewer.setCursor(Qt::OpenHandCursor);
}

void MainWindow::changeEvent(QEvent *e)
{
    //LOG_MODEL_DEBUG("MainWindow", "MainWindow::changeEvent.e->type:%d", e->type());
    switch(e->type())
    {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    }
}

int MainWindow::InitMenuTranslate()
{
    QMap<QString, _MENU> m;
    m["Default"] = {QLocale::system().name(), tr("Default")};
    m["en"] = {":/icon/English", tr("English")};
    m["zh_CN"] = {":/icon/China", tr("Chinese")};
    m["zh_TW"] = {":/icon/China", tr("Chinese(TaiWan)")};
    m["Default"].icon = m[QLocale::system().name()].icon;
    
    m_MenuTranslate.setIcon(QIcon(":/icon/Language"));
    m_MenuTranslate.setTitle(tr("Language(&L)"));
    ui->menuOptions->addMenu(&m_MenuTranslate);
    
    QMap<QString, _MENU>::iterator itMenu;
    for(itMenu = m.begin(); itMenu != m.end(); itMenu++)
    {
        _MENU v = itMenu.value();
        m_ActionTranslator[itMenu.key()] =
                m_MenuTranslate.addAction(
                    QIcon(v.icon), v.text);
    }
    
    QMap<QString, QAction*>::iterator it;
    for(it = m_ActionTranslator.begin(); it != m_ActionTranslator.end(); it++)
    {
        it.value()->setCheckable(true);
        m_ActionGroupTranslator.addAction(it.value());
    }

    LOG_MODEL_DEBUG("MainWindow",
                    "MainWindow::InitMenuTranslate m_ActionTranslator size:%d",
                    m_ActionTranslator.size());

    bool check = connect(&m_ActionGroupTranslator, SIGNAL(triggered(QAction*)),
                        SLOT(slotActionGroupTranslateTriggered(QAction*)));
    Q_ASSERT(check);

    QString szLocale = CGlobal::Instance()->GetLanguage();
    QAction* pAct = m_ActionTranslator[szLocale];
    if(pAct)
    {
        LOG_MODEL_DEBUG("MainWindow",
                        "MainWindow::InitMenuTranslate setchecked locale:%s",
                        szLocale.toStdString().c_str());
        pAct->setChecked(true);
        m_MenuTranslate.setIcon(pAct->icon());
        LOG_MODEL_DEBUG("MainWindow",
                        "MainWindow::InitMenuTranslate setchecked end");
    }
    
    return 0;
}

int MainWindow::ClearMenuTranslate()
{
    QMap<QString, QAction*>::iterator it;
    for(it = m_ActionTranslator.begin(); it != m_ActionTranslator.end(); it++)
    {
        m_ActionGroupTranslator.removeAction(it.value());
    }
    m_ActionGroupTranslator.disconnect();
    m_ActionTranslator.clear();
    m_MenuTranslate.clear();    

    LOG_MODEL_DEBUG("MainWindow",
                    "MainWindow::ClearMenuTranslate m_ActionTranslator size:%d",
                    m_ActionTranslator.size());
    
    return 0;
}

int MainWindow::ClearTranslate()
{
    if(!m_TranslatorQt.isNull())
    {
        qApp->removeTranslator(m_TranslatorQt.data());
        m_TranslatorQt.clear();
    }

    if(m_TranslatorApp.isNull())
    {
        qApp->removeTranslator(m_TranslatorApp.data());
        m_TranslatorApp.clear();
    }
    return 0;
}

int MainWindow::LoadTranslate(QString szLocale)
{
    if(szLocale.isEmpty())
    {
        szLocale = CGlobal::Instance()->GetLanguage();
    }

    if("Default" == szLocale)
    {
        szLocale = QLocale::system().name();
    }

    LOG_MODEL_DEBUG("main", "locale language:%s",
                    szLocale.toStdString().c_str());

    ClearTranslate();
    LOG_MODEL_DEBUG("MainWindow", "Translate dir:%s",
                    qPrintable(CGlobalDir::Instance()->GetDirTranslate()));

    m_TranslatorQt = QSharedPointer<QTranslator>(new QTranslator(this));
    m_TranslatorQt->load("qt_" + szLocale + ".qm",
                         CGlobalDir::Instance()->GetDirTranslate());
    qApp->installTranslator(m_TranslatorQt.data());

    m_TranslatorApp = QSharedPointer<QTranslator>(new QTranslator(this));
#ifdef ANDROID
    m_TranslatorApp->load(":/translations/app_" + szLocale + ".qm");
#else
    m_TranslatorApp->load("app_" + szLocale + ".qm",
                          CGlobalDir::Instance()->GetDirTranslate());
#endif
    qApp->installTranslator(m_TranslatorApp.data());

    ui->retranslateUi(this);
    return 0;
}

void MainWindow::slotActionGroupTranslateTriggered(QAction *pAct)
{
    LOG_MODEL_DEBUG("MainWindow", "MainWindow::slotActionGroupTranslateTriggered");
    QMap<QString, QAction*>::iterator it;
    for(it = m_ActionTranslator.begin(); it != m_ActionTranslator.end(); it++)
    {
        if(it.value() == pAct)
        {
            QString szLocale = it.key();
            CGlobal::Instance()->SetLanguage(szLocale);
            LoadTranslate(it.key());
            pAct->setChecked(true);
            InitMenuTranslate();
            QMessageBox::information(this, tr("Information"),
                                     tr("Change language must reset program."));
            close();
            
            return;
        }
    }
}

void MainWindow::on_actionExit_E_triggered()
{
    this->close();
}

void MainWindow::on_actionMeasure_the_distance_M_triggered()
{
    if(ui->actionMeasure_the_distance_M->isChecked())
    {
        osgViewer::Viewer* viewer = (osgViewer::Viewer*)m_MapViewer.getViewer();
        if(NULL == m_pMeasureTool)
            m_pMeasureTool = new CMeasureTool(viewer, m_Root, m_MapNode, this);
        QRect rect = this->centralWidget()->geometry();
        m_pMeasureTool->move(rect.left(), rect.top());
        m_pMeasureTool->show();
        m_MapViewer.setCursor(Qt::CrossCursor);
    }
    else
    {
        m_MapViewer.setCursor(Qt::OpenHandCursor);
        m_pMeasureTool->close();
        if(m_pMeasureTool)
        {
            delete m_pMeasureTool;
            m_pMeasureTool = NULL;
        }
    }
}

int MainWindow::InitMenuStyles()
{
    QMap<QString, QAction*>::iterator it;
    m_ActionStyles["Custom"] = m_MenuStyle.addAction(tr("Custom"));
    m_ActionStyles["System"] = m_MenuStyle.addAction(tr("System"));
    m_ActionStyles["Blue"] = m_MenuStyle.addAction(tr("Blue"));
    m_ActionStyles["Dark"] = m_MenuStyle.addAction(tr("Dark"));
    
    for(it = m_ActionStyles.begin(); it != m_ActionStyles.end(); it++)
    {
        it.value()->setCheckable(true);
        m_ActionGroupStyle.addAction(it.value());
    }
    bool check = connect(&m_ActionGroupStyle, SIGNAL(triggered(QAction*)),
                         SLOT(slotActionGroupStyleTriggered(QAction*)));
    Q_ASSERT(check);
    QAction* pAct = m_ActionStyles[CGlobal::Instance()->GetStyleMenu()];
    if(pAct)
    {
        pAct->setChecked(true);
    }
    m_MenuStyle.setIcon(QIcon(":/icon/Stype"));
    m_MenuStyle.setTitle(tr("Change Style Sheet(&S)"));
    ui->menuOptions->addMenu(&m_MenuStyle);
    return 0;
}

int MainWindow::ClearMenuStyles()
{
    QMap<QString, QAction*>::iterator it;
    for(it = m_ActionStyles.begin(); it != m_ActionStyles.end(); it++)
    {
        m_ActionGroupStyle.removeAction(it.value());
    }
    m_ActionGroupStyle.disconnect();
    m_ActionStyles.clear();
    m_MenuStyle.clear();
    return 0;
}

int MainWindow::LoadStyle()
{
    QString szFile = CGlobal::Instance()->GetStyle();
    if(szFile.isEmpty())
        qApp->setStyleSheet("");
    else
    {
        QFile file(szFile);
        if(file.open(QFile::ReadOnly))
        {
            QString stylesheet= file.readAll();
            qApp->setStyleSheet(stylesheet);
            file.close();
        }
        else
        {
            LOG_MODEL_ERROR("app", "file open file [%s] fail:%d",
                        CGlobal::Instance()->GetStyle().toStdString().c_str(),
                        file.error());
        }
    }
    return 0;
}

int MainWindow::OpenCustomStyleMenu()
{
    QString szFile;
    QString szFilter("*.qss *.*");
    szFile = CTool::FileDialog(this, QString(), szFilter, tr("Open File"));
    if(szFile.isEmpty())
        return -1;

    QFile file(szFile);
    if(file.open(QFile::ReadOnly))
    {
        QString stylesheet= file.readAll();
        qApp->setStyleSheet(stylesheet);
        file.close();
        QSettings conf(CGlobalDir::Instance()->GetApplicationConfigureFile(),
                       QSettings::IniFormat);
        conf.setValue("UI/StyleSheet", szFile);
        
        CGlobal::Instance()->SetStyleMenu("Custom", szFile);
    }
    else
    {
        LOG_MODEL_ERROR("app", "file open file [%s] fail:%d", 
                        szFile.toStdString().c_str(), file.error());
    }
    return 0;
}

void MainWindow::slotActionGroupStyleTriggered(QAction* act)
{
    QMap<QString, QAction*>::iterator it;
    for(it = m_ActionStyles.begin(); it != m_ActionStyles.end(); it++)
    {
        if(it.value() == act)
        {
            act->setChecked(true);
            if(it.key() == "Blue")
                CGlobal::Instance()->SetStyleMenu("Blue", ":/sink/Blue");
            else if(it.key() == "Dark")
                CGlobal::Instance()->SetStyleMenu("Dark", ":/qdarkstyle/style.qss");
            else if(it.key() == "Custom")
                OpenCustomStyleMenu();
            else
                CGlobal::Instance()->SetStyleMenu("System", "");
        }
    }

    LoadStyle();
}

void MainWindow::on_actionStatusBar_S_triggered()
{
    bool bVisable = !CGlobal::Instance()->GetStatusbarVisable();
    CGlobal::Instance()->SetStatusbarVisable(bVisable);
    ui->actionStatusBar_S->setChecked(bVisable);
    this->statusBar()->setVisible(bVisable);
}

void MainWindow::on_actionToolBar_triggered()
{
    bool bVisable = !CGlobal::Instance()->GetToolbarVisable();
    CGlobal::Instance()->SetToolbarVisable(bVisable);
    ui->actionToolBar->setChecked(bVisable);
    ui->mainToolBar->setVisible(bVisable);
}

int MainWindow::InitToolbar()
{
    ui->mainToolBar->setVisible(CGlobal::Instance()->GetToolbarVisable());
    ui->actionToolBar->setChecked(CGlobal::Instance()->GetToolbarVisable());
    
    ui->mainToolBar->addAction(ui->actionOpen_Project);
    ui->mainToolBar->addAction(ui->actionSava_project_S);
    ui->mainToolBar->addAction(ui->actionOpen_track_T);
    ui->mainToolBar->addSeparator();
    ui->mainToolBar->addAction(ui->actionMeasure_the_distance_M);
    return 0;
}

void MainWindow::on_actionAbout_A_triggered()
{
    CDlgAbout about(this);
    about.exec();
}

void MainWindow::slotActionGroupMapTriggered(QAction *act)
{
    if(!m_MapNode.valid())
        return;
    
    osg::ref_ptr<osgEarth::Map> map = m_MapNode->getMap();
    int i = 0;
    std::list<QAction*>::iterator it;
    for(it = m_ActionMap.begin(); it != m_ActionMap.end(); it++)
    {
        osgEarth::ImageLayer* layer = map->getLayerAt<osgEarth::ImageLayer>(i);
        if(*it == act)
        {
            layer->setVisible(true);
        }
        else
            layer->setVisible(false);
        i++;
    }
}

void MainWindow::slotMenuMapShow()
{
    if(!m_MapNode.valid())
        return;

    ClearMenuMap();
    osg::ref_ptr<osgEarth::Map> map = m_MapNode->getMap();
    std::vector< osg::ref_ptr<osgEarth::ImageLayer> > v;
    map->getLayers(v);
    std::vector< osg::ref_ptr<osgEarth::ImageLayer> >::iterator it;
    for(it = v.begin(); it != v.end(); it++)
    {
        osg::ref_ptr<osgEarth::ImageLayer> image = *it;
        QAction* action =
                ui->menuMap_A->addAction(QString(image->getName().c_str()));
        action->setCheckable(true);
        action->setChecked(image->getVisible());
        m_ActionMap.push_back(action);
        m_ActionGroupMap.addAction(action);
    }
    bool check = connect(&m_ActionGroupMap, SIGNAL(triggered(QAction*)),
                         SLOT(slotActionGroupMapTriggered(QAction*)));
    Q_ASSERT(check);
    
    return;
}

void MainWindow::ClearMenuMap()
{
    m_ActionGroupMap.disconnect();
    std::list<QAction*>::iterator it;
    for(it = m_ActionMap.begin(); it != m_ActionMap.end(); it++)
    {
        m_ActionGroupMap.removeAction(*it);
    }
    m_ActionMap.clear();
    ui->menuMap_A->clear();
        
    return;
}
