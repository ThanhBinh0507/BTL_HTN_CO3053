module.exports = {
    flowFile: 'flows.json',
    flowFilePretty: true,
    credentialSecret: false,
    adminAuth: null,
    uiPort: process.env.PORT || 1880,
    diagnostics: {
        enabled: true
    },
    logging: {
        console: {
            level: "info",
            metrics: false,
            audit: false
        }
    },
    exportGlobalContextKeys: false,
    editorTheme: {
        projects: {
            enabled: false
        }
    }
};

