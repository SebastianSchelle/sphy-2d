#include "dm-general.hpp"
#include "event-listener.hpp"
#include "ptr-handle.hpp"
#include <dm-if.hpp>
#include <user-interface.hpp>

namespace ui
{

void DmIf::setupDataModels()
{
    setupMenu();
    setupTips();
    // setupLoadGame();
}

void DmIf::postInit()
{
    evLoadGame =
        PageEventListener({.onShow = [this]()
                           {
                               LG_E("Check save dir");
                               menu.data.load.saves.clear();
                               vector<sphyc::SaveInfo> saves;
                               ptrHandle->saveManager->listSaveInfos(saves);
                               for (auto save : saves)
                               {
                                   menu.data.load.saves.push_back(save);
                                   LG_D("safe: {}", save.name);
                               }
                           }});
    ui->addPageEvents("menu-load-game", evLoadGame);
}

void DmIf::setupMenu()
{
    auto constMenu = ui->getDataModel("menu");
    DmWindow<DmMenu>::RegisterType(constMenu);
    menu = DmWindow<DmMenu>{
        .title = "[menu.title]",
        .movable = false,
        .closable = false,
    };
    hMenu = constMenu.GetModelHandle();
    menu.addButton(
        constMenu,
        {.id = "btnExit", .label = "[btn.exit]", .tooltip = "btn.exit.tooltip"},
        [this]() { ptrHandle->client->shutdown(); });
    menu.addButton(constMenu,
                   {.id = "btnOptions",
                    .label = "[btn.options]",
                    .tooltip = "btn.options.tooltip",
                    .disabled = true},
                   [this]() { ui->menuPush("menu-options", "[btn.options]"); });
    menu.addButton(constMenu,
                   {.id = "btnNewGame",
                    .label = "[btn.newgame]",
                    .tooltip = "btn.newgame.tooltip"},
                   [this]()
                   { ui->menuPush("menu-new-game", "[btn.newgame]"); });
    menu.addButton(constMenu,
                   {.id = "btnContinueGame",
                    .label = "[btn.continuegame]",
                    .tooltip = "btn.continuegame.tooltip"},
                   [this]()
                   {
                       vector<sphyc::SaveInfo> safes;
                       ptrHandle->saveManager->listSaveInfos(safes);
                       for (auto safe : safes)
                       {
                           LG_D("safe: {}", safe.name);
                       }
                   });
    menu.addButton(constMenu,
                   {.id = "btnLoadGame",
                    .label = "[btn.loadgame]",
                    .tooltip = "btn.loadgame.tooltip"},
                   [this]()
                   { ui->menuPush("menu-load-game", "[btn.loadgame]"); });
    // todo: script hook (modding) for registering menu elements in the data
    // model
    menu.setup(constMenu, hMenu, {.onClose = [this]() { ui->menuHide(); }});
}

void DmIf::setupTips()
{
    auto constTips = ui->getDataModel("tips");
    DmWindow<DmTips>::RegisterType(constTips);
    tips = DmWindow<DmTips>{.title = "[tips.title]",
                            .movable = false,
                            .closable = false,
                            .data = {.tip = "[lorem400]"}};
    hTips = constTips.GetModelHandle();
    tips.setup(constTips, hTips, {});
}

// void DmIf::setupLoadGame()
// {
//     // auto constLoadGame = ui->getDataModel("loadgame");
//     // DmWindow<DmLoadGame>::RegisterType(constLoadGame);
//     // loadGame = DmWindow<DmLoadGame>{.title = "[loadgame.title]",
//     //                                 .movable = false,
//     //                                 .closable = false,
//     //                                 .data = DmLoadGame{}};
//     // hLoadGame = constLoadGame.GetModelHandle();
// loadGame.setup(constLoadGame, hLoadGame, {});

// }

}  // namespace ui
