const touchArea = document.getElementById('touch-area');
const status = document.getElementById('status');
const pageTitle = document.getElementById('page-title');
const navBar = document.getElementById('nav-bar');
const infoContainer = document.getElementById('page-info-container');
const BUTTON_GAP_PX = 8;

function toPercent(value) {
    return `${(value / 10).toFixed(4)}%`;
}

function positionButton(el, btn) {
    el.style.left = `calc(${toPercent(btn.x1)} + ${BUTTON_GAP_PX}px)`;
    el.style.top = `calc(${toPercent(btn.y1)} + ${BUTTON_GAP_PX}px)`;
    el.style.width = `calc(${toPercent(btn.x2 - btn.x1)} - ${BUTTON_GAP_PX * 2}px)`;
    el.style.height = `calc(${toPercent(btn.y2 - btn.y1)} - ${BUTTON_GAP_PX * 2}px)`;
}

function sendRegionClick(btn) {
    const x = Math.round((btn.x1 + btn.x2) / 2);
    const y = Math.round((btn.y1 + btn.y2) / 2);
    sendClick(x, y);
}

function createButton(btn, index) {
    const el = document.createElement('button');
    el.className = 'button';
    el.id = `btn-${index}`;
    el.type = 'button';
    el.innerText = btn.label;
    positionButton(el, btn);
    el.addEventListener('touchstart', (e) => {
        e.preventDefault();
        e.stopPropagation();
        sendRegionClick(btn);
    }, { passive: false });
    el.addEventListener('mousedown', (e) => {
        e.preventDefault();
        e.stopPropagation();
        sendRegionClick(btn);
    });
    return el;
}

function renderButtons(buttons) {
    touchArea.replaceChildren();
    buttons.forEach((btn, index) => {
        touchArea.appendChild(createButton(btn, index));
    });
}

function updatePageInfo() {
    fetch('/page_info')
    .then(response => response.json())
    .then(data => {
        pageTitle.innerText = data.page_name;

        // ナビゲーションボタンの生成/更新
        // 既存のボタンを削除（infoContainer以外）
        const existingBtns = navBar.querySelectorAll('.nav-btn');
        existingBtns.forEach(btn => btn.remove());

        for (let i = 0; i < data.total_pages; i++) {
            const btn = document.createElement('button');
            btn.className = 'nav-btn';
            btn.innerText = (i + 1);
            btn.onclick = () => setPage(i);
            if (i === data.current_page) {
                btn.disabled = true;
            }
            infoContainer.before(btn);
        }

        renderButtons(data.buttons);
    });
}

function setPage(index) {
    fetch(`/set_page/${index}`, { method: 'POST' })
    .then(() => updatePageInfo());
}

function sendClick(x, y) {
    status.innerText = `Sending: (${x}, ${y})`;
    fetch('/click', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ x: x, y: y }),
    })
    .then(response => response.json())
    .then(data => {
        status.innerText = `Executed: ${data.command}`;
        setTimeout(() => { status.innerText = 'Ready'; }, 1500);
    });
}

touchArea.addEventListener('touchstart', function(e) {
    e.preventDefault();
    const touch = e.touches[0];
    const rect = touchArea.getBoundingClientRect();
    const x = Math.round((touch.clientX - rect.left) / rect.width * 1000);
    const y = Math.round((touch.clientY - rect.top) / rect.height * 1000);
    sendClick(x, y);
}, { passive: false });

touchArea.addEventListener('mousedown', function(e) {
    const rect = touchArea.getBoundingClientRect();
    const x = Math.round((e.clientX - rect.left) / rect.width * 1000);
    const y = Math.round((e.clientY - rect.top) / rect.height * 1000);
    sendClick(x, y);
});

// 初期化
updatePageInfo();
