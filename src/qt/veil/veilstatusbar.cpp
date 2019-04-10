#include <qt/veil/veilstatusbar.h>
#include <qt/veil/forms/ui_veilstatusbar.h>

#include <qt/bitcoingui.h>
#include <qt/walletmodel.h>
#include <qt/veil/qtutils.h>
#include <iostream>

VeilStatusBar::VeilStatusBar(QWidget *parent, BitcoinGUI* gui) :
    QWidget(parent),
    mainWindow(gui),
    ui(new Ui::VeilStatusBar)
{
    ui->setupUi(this);

    connect(ui->btnLock, SIGNAL(clicked()), this, SLOT(onBtnLockClicked()));
    connect(ui->btnSync, SIGNAL(clicked()), this, SLOT(onBtnSyncClicked()));
}

bool VeilStatusBar::getSyncStatusVisible() {
    return ui->btnSync->isVisible();
}

void VeilStatusBar::updateSyncStatus(QString status){
    ui->btnSync->setText(status);
}

void VeilStatusBar::setSyncStatusVisible(bool fVisible) {
    ui->btnSync->setVisible(fVisible);
}

void VeilStatusBar::onBtnSyncClicked(){
    mainWindow->showModalOverlay();
}

bool fBlockNextStakeCheckSignal = false;
void VeilStatusBar::onCheckStakingClicked(bool res) {
    // When our own dialog internally changes the checkstate, block signal from executing
    if (fBlockNextStakeCheckSignal) {
        fBlockNextStakeCheckSignal = false;
        return;
    }

    // Miner thread starts in init.cpp, but staking enabled flag is checked each iteration of the miner, so can be enabled or disabled here
    WalletModel::EncryptionStatus lockState = walletModel->getEncryptionStatus();
    if (res){
        if (gArgs.GetBoolArg("-exchangesandservicesmode", false) || lockState == WalletModel::Locked) {
            QString dialogMsg = gArgs.GetBoolArg("-exchangesandservicesmode", false) ? "Staking is disabled in exchange mode" : "Must unlock wallet before staking can be enabled";
            openToastDialog(dialogMsg, mainWindow);
            fBlockNextStakeCheckSignal = true;
            ui->checkStaking->setChecked(false);
            return;
        }else{
            this->walletModel->setStakingEnabled(true);
            mainWindow->updateWalletStatus();
            openToastDialog("Staking enabled", mainWindow);
        }
    } else {
        this->walletModel->setStakingEnabled(false);
        mainWindow->updateWalletStatus();
        openToastDialog("Staking disabled", mainWindow);
    }

}

bool fBlockNextPrecomputeCheckSignal = false;
void VeilStatusBar::onCheckPrecomputeClicked(bool res) {

    // When our own dialog internally changes the checkstate, block signal from executing
    if (fBlockNextPrecomputeCheckSignal) {
        fBlockNextPrecomputeCheckSignal = false;
        return;
    }

    WalletModel::EncryptionStatus lockState = walletModel->getEncryptionStatus();

    if (res){
        if (gArgs.GetBoolArg("-exchangesandservicesmode", false) || lockState == WalletModel::Locked) {
            QString dialogMsg = gArgs.GetBoolArg("-exchangesandservicesmode", false) ? "Precomputing is disabled in exchange mode" : "Must unlock wallet before precomputing can be enabled";
            openToastDialog(dialogMsg, mainWindow);
            fBlockNextPrecomputeCheckSignal = true;
            ui->checkPrecompute->setChecked(false);
            return;
        } else {
            std::string strStatus;
            if(!this->walletModel->StartPrecomputing(strStatus)) {
                openToastDialog("Failed to start precomputing", mainWindow);
                this->walletModel->setPrecomputingEnabled(false);
                return;
            }
            this->walletModel->setPrecomputingEnabled(true);
            mainWindow->updateWalletStatus();
            openToastDialog("Precomputing enabled", mainWindow);
        }
    } else {
        this->walletModel->setPrecomputingEnabled(false);
        this->walletModel->StopPrecomputing();
        mainWindow->updateWalletStatus();
        openToastDialog("Precomputing disabled", mainWindow);
    }

}

bool fBlockNextBtnLockSignal = false;
void VeilStatusBar::onBtnLockClicked()
{
    // When our own dialog internally changes the checkstate, block signal from executing
    if (fBlockNextBtnLockSignal) {
        fBlockNextBtnLockSignal = false;
        return;
    }

    if(walletModel->getEncryptionStatus() == WalletModel::Unlocked || walletModel->getEncryptionStatus() == WalletModel::UnlockedForStakingOnly){
        if (walletModel->setWalletLocked(true, false)){
            ui->btnLock->setIcon(QIcon(":/icons/ic-locked-png"));
            openToastDialog("Wallet locked", mainWindow);
        }else{
            openToastDialog("Wallet not locked", mainWindow);
        }
    }else{
        bool isLocked = walletModel->getEncryptionStatus() == WalletModel::Locked;
        if (isLocked) {
            mainWindow->showHide(true);
            if(!unlockPasswordDialog)
                unlockPasswordDialog = new UnlockPasswordDialog(/*fUnlockForStakingOnly*/false, walletModel, mainWindow);
            if (openDialogWithOpaqueBackground(unlockPasswordDialog, mainWindow, 4)) {
                mainWindow->updateWalletStatus();
                ui->btnLock->setIcon(QIcon(":/icons/ic-unlocked-png"));
                openToastDialog("Wallet unlocked", mainWindow);
            } else {
                openToastDialog("Wallet failed to unlock", mainWindow);
            }
        } else {
            if(mainWindow->encryptWallet(true)){
                ui->btnLock->setIcon(QIcon(":/icons/ic-locked-png"));
                openToastDialog("Wallet locked", mainWindow);
            }
        }

    }

    updateStakingCheckbox();
    updatePrecomputeCheckbox();
}

void VeilStatusBar::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
    connect(ui->checkStaking, SIGNAL(toggled(bool)), this, SLOT(onCheckStakingClicked(bool)));
    connect(ui->checkPrecompute, SIGNAL(toggled(bool)), this, SLOT(onCheckPrecomputeClicked(bool)));
    WalletModel::EncryptionStatus lockState = walletModel->getEncryptionStatus();
    bool lockStatus = lockState == WalletModel::Locked || lockState == WalletModel::UnlockedForStakingOnly;
    ui->btnLock->setIcon(QIcon( (lockStatus) ? ":/icons/ic-locked-png" : ":/icons/ic-unlocked-png"));
    updateStakingCheckbox();
    updatePrecomputeCheckbox();
    updateLockCheckbox();
}

void VeilStatusBar::updateLockCheckbox(){
    if(walletModel) {
        WalletModel::EncryptionStatus lockState = walletModel->getEncryptionStatus();
        bool lockStatus = lockState == WalletModel::Locked || lockState == WalletModel::UnlockedForStakingOnly;
        if (ui->btnLock->isChecked() != lockStatus) {
            ui->btnLock->setChecked(lockStatus);
            ui->btnLock->setIcon(QIcon( (lockStatus) ? ":/icons/ic-locked-png" : ":/icons/ic-unlocked-png"));

            QString strToolTip = tr("Wallet is locked for all transaction types.");
            if (lockState == WalletModel::UnlockedForStakingOnly)
                strToolTip = tr("Wallet is unlocked for staking transactions only.");
            ui->btnLock->setStatusTip(strToolTip);
            ui->btnLock->setToolTip(strToolTip);

            fBlockNextBtnLockSignal = true;
        }
    }
}

void VeilStatusBar::updateStakingCheckbox()
{
    if(walletModel) {
        WalletModel::EncryptionStatus lockState = walletModel->getEncryptionStatus();
        bool stakingStatus = walletModel->isStakingEnabled() && lockState != WalletModel::Locked;
        if (ui->checkStaking->isChecked() != stakingStatus) {
            fBlockNextStakeCheckSignal = true;
            ui->checkStaking->setChecked(stakingStatus);
            return;
        }
    }
}

void VeilStatusBar::updatePrecomputeCheckbox()
{
    if(walletModel) {
        WalletModel::EncryptionStatus lockState = walletModel->getEncryptionStatus();
        bool precomputeStatus = walletModel->isPrecomputingEnabled() && lockState != WalletModel::Locked;
        if (ui->checkPrecompute->isChecked() != precomputeStatus) {
            fBlockNextPrecomputeCheckSignal = true;
            ui->checkPrecompute->setChecked(precomputeStatus);
            return;
        }
    }
}

VeilStatusBar::~VeilStatusBar()
{
    delete ui;
}
