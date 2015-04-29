%include('header.tpl', title="")

%include('menu.tpl', current='list')

<table>
  <tr>
    <th>Name</th>
    <th>State</th>
    <th>UUID</th>
    <th>Sender URI</th>
  </tr>
%for receiver in receivers:
  <tr>
    <td>{{receiver[0]}}</td>
    <td>{{receiver[1]}}</td>
    <td>{{receiver[2]}}</td>
    <td>{{receiver[3]}}</td>
  </tr>
%end
</table>

%include('footer.tpl')
