#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/LevelPage.hpp>

using namespace geode::prelude;

constexpr size_t MAX_LEVELS = 22;

static std::vector<int> g_activeLevels;

static std::vector<int> loadSavedLevels() {
    return Mod::get()->getSavedValue<std::vector<int>>("levels", {});
}

$on_mod(Loaded) {
    g_activeLevels = loadSavedLevels();
}

class LevelFetcher : public LevelManagerDelegate {
public:
    int m_targetID = 0;
    bool m_loading = false;

    static LevelFetcher* get() {
        static LevelFetcher instance;
        return &instance;
    }

    void fetch(int id) {
        if (m_loading) return;
        m_loading = true;
        m_targetID = id;

        Notification::create("Loading level...", NotificationIcon::Loading, 1.f)->show();

        auto glm = GameLevelManager::get();
        glm->m_levelManagerDelegate = this;
        auto search = GJSearchObject::create(SearchType::Search, std::to_string(id));
        glm->getOnlineLevels(search);
    }

    void finish() {
        auto glm = GameLevelManager::get();
        if (glm->m_levelManagerDelegate == this) glm->m_levelManagerDelegate = nullptr;
        m_loading = false;
    }

    void onLoaded(CCArray* levels) {
        finish();
        if (!levels || levels->count() == 0) return onFailed();

        GJGameLevel* found = nullptr;
        for (auto level : CCArrayExt<GJGameLevel*>(levels)) {
            if (level->m_levelID.value() == m_targetID) { found = level; break; }
        }
        if (!found) found = static_cast<GJGameLevel*>(levels->objectAtIndex(0));

        auto scene = LevelInfoLayer::scene(found, false);
        CCDirector::get()->pushScene(CCTransitionFade::create(0.5f, scene));
    }

    void onFailed() {
        finish();
        FLAlertLayer::create(
            "Error",
            fmt::format("Couldn't load level <cy>{}</c>.\nCheck the ID and your internet connection.", m_targetID),
            "OK"
        )->show();
    }

    void loadLevelsFinished(CCArray* levels, char const*) override { onLoaded(levels); }
    void loadLevelsFinished(CCArray* levels, char const*, int) override { onLoaded(levels); }
    void loadLevelsFailed(char const*) override { onFailed(); }
    void loadLevelsFailed(char const*, int) override { onFailed(); }
};

class CustomEditorPopup : public Popup<> {
protected:
    ScrollLayer* m_scroll = nullptr;
    CCLabelBMFont* m_countLabel = nullptr;
    std::vector<TextInput*> m_inputs;

    bool setup() override {
        this->setTitle("Custom Main Levels");
        auto size = m_mainLayer->getContentSize();

        m_scroll = ScrollLayer::create({ size.width - 30.f, size.height - 95.f });
        m_scroll->setPosition({ 15.f, 50.f });
        m_scroll->m_contentLayer->setLayout(
            ColumnLayout::create()
                ->setAxisReverse(true)
                ->setAxisAlignment(AxisAlignment::End)
                ->setAutoGrowAxis(m_scroll->getContentHeight())
                ->setGap(6.f)
        );
        m_mainLayer->addChild(m_scroll);

        m_countLabel = CCLabelBMFont::create("", "chatFont.fnt");
        m_countLabel->setScale(0.7f);
        m_countLabel->setPosition({ size.width / 2.f, size.height - 38.f });
        m_mainLayer->addChild(m_countLabel);

        auto menu = CCMenu::create();
        menu->setPosition({ 0.f, 0.f });
        menu->setContentSize(size);
        m_mainLayer->addChild(menu);

        auto addBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Add", "goldFont.fnt", "GJ_button_01.png", 0.8f),
            this, menu_selector(CustomEditorPopup::onAdd)
        );
        addBtn->setPosition({ size.width / 2.f - 55.f, 25.f });
        menu->addChild(addBtn);

        auto uploadBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Upload", "goldFont.fnt", "GJ_button_02.png", 0.8f),
            this, menu_selector(CustomEditorPopup::onUpload)
        );
        uploadBtn->setPosition({ size.width / 2.f + 55.f, 25.f });
        menu->addChild(uploadBtn);

        auto saved = loadSavedLevels();
        if (saved.empty()) {
            this->addRow(0);
        } else {
            for (int id : saved) this->addRow(id);
        }
        return true;
    }

    void addRow(int id) {
        if (m_inputs.size() >= MAX_LEVELS) {
            Notification::create("Max 22 levels!", NotificationIcon::Warning)->show();
            return;
        }

        float width = m_scroll->getContentWidth();
        auto row = CCNode::create();
        row->setContentSize({ width, 30.f });

        auto label = CCLabelBMFont::create(fmt::format("Level {}", m_inputs.size() + 1).c_str(), "goldFont.fnt");
        label->setScale(0.55f);
        label->setAnchorPoint({ 0.f, 0.5f });
        label->setPosition({ 5.f, 15.f });
        row->addChild(label);

        auto input = TextInput::create(140.f, "Level ID");
        input->setCommonFilter(CommonFilter::Uint);
        input->setMaxCharCount(10);
        if (id > 0) input->setString(std::to_string(id));
        input->setPosition({ width - 80.f, 15.f });
        row->addChild(input);

        m_inputs.push_back(input);
        m_scroll->m_contentLayer->addChild(row);
        m_scroll->m_contentLayer->updateLayout();
        this->updateCount();
    }

    void updateCount() {
        m_countLabel->setString(fmt::format("{} / {}", m_inputs.size(), MAX_LEVELS).c_str());
    }

    void onAdd(CCObject*) {
        this->addRow(0);
        m_scroll->scrollToTop();
    }

    void onUpload(CCObject*) {
        std::vector<int> ids;
        for (auto input : m_inputs) {
            auto text = input->getString();
            if (text.empty()) continue;
            auto num = numFromString<int>(text);
            if (num && num.unwrap() > 0) ids.push_back(num.unwrap());
        }

        if (ids.empty()) {
            FLAlertLayer::create("Oops", "Enter at least one level ID!", "OK")->show();
            return;
        }

        Mod::get()->setSavedValue("levels", ids);
        (void)Mod::get()->saveData();

        FLAlertLayer::create(
            "Uploaded!",
            fmt::format("Saved <cg>{}</c> level(s).\n<cy>Restart the game</c> to play your custom main levels!", ids.size()),
            "OK"
        )->show();
    }

public:
    static CustomEditorPopup* create() {
        auto ret = new CustomEditorPopup();
        if (ret->initAnchored(320.f, 260.f)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

class $modify(CustomMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        auto label = CCLabelBMFont::create("Edit", "bigFont.fnt");
        label->setScale(0.4f);
        auto spr = CircleButtonSprite::create(label, CircleBaseColor::Green, CircleBaseSize::MediumAlt);

        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(CustomMenuLayer::onCustomEditor));
        btn->setID("custom-editor-button"_spr);

        if (auto menu = this->getChildByID("bottom-menu")) {
            menu->addChild(btn);
            menu->updateLayout();
        }
        return true;
    }

    void onCustomEditor(CCObject*) {
        CustomEditorPopup::create()->show();
    }
};

class $modify(CustomLevelPage, LevelPage) {
    void onPlay(CCObject* sender) {
        if (!m_level) return LevelPage::onPlay(sender);

        int mainID = m_level->m_levelID.value();
        if (mainID < 1 || mainID > static_cast<int>(g_activeLevels.size())) {
            return LevelPage::onPlay(sender);
        }

        LevelFetcher::get()->fetch(g_activeLevels[mainID - 1]);
    }
};
