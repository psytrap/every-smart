

/// Open the app page
function openPage() {
  let url = chrome.runtime.getURL("every-smart.html");
  chrome.tabs.create({
    url: url,
  });
}

/// Add button to Chrome
chrome.action.onClicked.addListener(openPage);

