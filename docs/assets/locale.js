(function () {
  const params = new URLSearchParams(window.location.search);
  if (params.has('lang')) return;

  const languages = (navigator.languages && navigator.languages.length)
    ? navigator.languages
    : [navigator.language || navigator.userLanguage || ''];
  const timezone = Intl.DateTimeFormat().resolvedOptions().timeZone || '';
  const region = (navigator.language || '').split('-')[1] || '';

  const looksJapanese = languages.some((lang) => /^ja(?:-|$)/i.test(lang))
    || /^JP$/i.test(region)
    || timezone === 'Asia/Tokyo'
    || timezone === 'Japan';

  if (!looksJapanese) return;

  params.set('lang', 'ja');
  const nextUrl = `${window.location.pathname}?${params.toString()}${window.location.hash}`;
  window.location.replace(nextUrl);
})();
