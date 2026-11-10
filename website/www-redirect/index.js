// Permanently redirect www.stillmar.com to stillmar.com, keeping the path and query
export default {
  fetch(request) {
    const url = new URL(request.url);
    url.hostname = "stillmar.com";
    return Response.redirect(url.toString(), 301);
  },
};
