#include "dm-general.hpp"
#include "event-listener.hpp"
#include "ptr-handle.hpp"
#include "save-manager.hpp"
#include "widgets.hpp"
#include <dm-if.hpp>
#include <main-window.hpp>
#include <model.hpp>
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
    evLoadGame = PageEventListener(
        {.onShow = [this]()
         {
             LG_E("Check save dir");
             menu.data.load.saves.clear();
             vector<sphyc::SaveInfo> saves;
             ptrHandle->saveManager->listSaveInfos(saves);
             for (auto save : saves)
             {
                 menu.data.load.saves.push_back(
                     {.button =
                          widget::Button{
                              .id = "loadgame",
                              .args = save.id,
                              .label = save.name,
                              .tooltip = "btn.options.tooltip",
                          },
                      .path = save.path});
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
        [this](const vector<string>& args) { ptrHandle->mainWin->shutdown(); });
    menu.addButton(constMenu,
                   {.id = "btnMainmenu",
                    .label = "[btn.mainmenu]",
                    .tooltip = "btn.mainmenu.tooltip"},
                   [this](const vector<string>& args)
                   { ptrHandle->mainWin->shutdown(); });
    menu.addButton(constMenu,
                   {.id = "btnOptions",
                    .label = "[btn.options]",
                    .tooltip = "btn.options.tooltip",
                    .disabled = false},
                   [this](const vector<string>& args)
                   {
                       ptrHandle->mainWin->shutdownLocalServer();
                       ptrHandle->client->shutdown();
                       // ui->menuPush("menu-options", "[btn.options]");
                   });
    menu.addButton(constMenu,
                   {.id = "btnNewGame",
                    .label = "[btn.newgame]",
                    .tooltip = "btn.newgame.tooltip"},
                   [this](const vector<string>& args)
                   { ui->menuPush("menu-new-game", "[btn.newgame]"); });
    menu.addButton(constMenu,
                   {.id = "btnContinueGame",
                    .args = "Hello, World",
                    .label = "[btn.continuegame]",
                    .tooltip = "btn.continuegame.tooltip"},
                   [this](const vector<string>& args)
                   {
                       sphyc::SaveInfo save;
                       if (ptrHandle->saveManager->getLastSaved(save))
                       {
                           LG_D("Load save from {}", save.path);
                           ptrHandle->mainWin->startLocalGame(save.path);
                       }
                   });
    menu.addButton(constMenu,
                   {.id = "btnLoadGame",
                    .label = "[btn.loadgame]",
                    .tooltip = "btn.loadgame.tooltip"},
                   [this](const vector<string>& args)
                   { ui->menuPush("menu-load-game", "[btn.loadgame]"); });
    // todo: script hook (modding) for registering menu elements in the data
    // model
    menu.setup(constMenu, hMenu, {.onClose = [this]() { ui->menuHide(); }});

    menu.eventListener.registerOnclickFun(
        "loadgame",
        [this](const vector<string>& args)
        {
            if (args.size())
            {
                const string id = args[0];
                LG_D("Load game with id {}", id);
                for (auto& save : menu.data.load.saves)
                {
                    if (save.button.args.contains(id))
                    {
                        LG_D("Load save from {}", save.path);
                        ptrHandle->mainWin->startLocalGame(
                            save.path, sphyc::AfterConnectState::Game);
                        break;
                    }
                }
            }
        });
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
